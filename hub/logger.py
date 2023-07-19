import json
import socket
import threading
import struct
from collections import namedtuple, defaultdict
import time
from pathlib import Path
from datetime import datetime
from zeroconf import Zeroconf, ServiceInfo

LISTEN_IP = "0.0.0.0"
UDP_PORT = 5005


def get_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(0)
    try:
        # doesn't even have to be reachable
        s.connect(('10.254.254.254', 1))
        IP = s.getsockname()[0]
    except Exception:
        IP = '127.0.0.1'
    finally:
        s.close()
    return IP


DEVICES = defaultdict(lambda: {'t0': time.time(), 'points': []})

messageStruct = struct.Struct("L ffffffff fff I")
fieldNames = ("MAC", "inputVoltage", "outputVoltage", "inputCurrent", "outputCurrent", "fetTemperature", "outputPower", "samplesPerSecond", "outputCharge", "minInputVoltage", "maxOutputVoltage", "maxOutputCurrent", "status")
stateBitNames = ("STATE_SENSORS_READY", "STATE_SENSORS_READ_EVENT", "STATE_BUCK_MODE_CIV", "STATE_BUCK_MODE_COV", "STATE_BUCK_MODE_COC", 
                "STATE_BUCK_MODE_SAFED", "STATE_BACKFLOW_ENABLED", "STATE_NETWORK_CONNECTED")

cumulativeFields = "outputCharge", "MAC", 
averagingFields = set(fieldNames) - set(cumulativeFields) - {"status"}


sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((LISTEN_IP, UDP_PORT))


class TelemetryFrame(namedtuple('TelemetryFrame', ('timestamp',)+fieldNames[:-1]+stateBitNames)):
    def __new__(cls, raw=None, **kwargs) -> None:
        if raw:
            unpacked = messageStruct.unpack(raw)
            state = unpacked[-1]
            return super().__new__(cls, time.time(), *unpacked[:-1], *(bool(state & 1<<i) for i in range(len(stateBitNames))))
        return super().__new__(cls, **kwargs)

def logFrame(frame):
    device = DEVICES[frame.MAC]
    if frame.timestamp - device['t0'] > 60:
        kwargs = {'timestamp': device['t0']}
        DT = device['points'][-1].timestamp - device['t0']
        
        print(frame.MAC, DT)
        
        for name in averagingFields:
            t = device['t0']
            total = 0
            for point in device['points']:
                total += (point.timestamp-t)*getattr(point, name)
                t = point.timestamp
            kwargs[name] = total/DT
            
        
        for name in cumulativeFields:
            kwargs[name] = getattr(device['points'][-1], name)
            
        for name in stateBitNames:
            t = device['t0']
            total = 0
            for point in device['points']:
                if getattr(point, name):
                    total+= point.timestamp-t
                kwargs[name] = total/DT
                t = point.timestamp
        
        averageFrame = TelemetryFrame(**kwargs)
        
        Path(f"./telemetryLog/{frame.MAC:x}").mkdir(parents=True, exist_ok=True)
        with open(f'./telemetryLog//{frame.MAC:x}/{datetime.fromtimestamp(device["t0"]).strftime("%Y-%m-%d")}.csv', 'a') as out:
            print(*averageFrame, sep='\t', file=out)
        
        device['t0'] = frame.timestamp
        device['points'] = [frame]
    else:
        device['points'].append(frame)

def forwardFrame(frame):
    message = json.dumps(frame._asdict())
    sock.sendto(message.encode(), ("192.168.1.104", 5005))


def receiver():
    
    while True:
        data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
        
        frame = TelemetryFrame(data)
        
        logFrame(frame)
        forwardFrame(frame)



if __name__ == '__main__':
    
    zeroconf = Zeroconf()
    try:
        zeroconf.register_service(ServiceInfo(
            '_mppt-hub._udp.local.',
            'MD MPPT telemetry target._mppt-hub._udp.local.',
            port=UDP_PORT,
            server=socket.getfqdn()+'.local',
            addresses=[get_ip()]
        ))
        receiver()
    finally:
        zeroconf.unregister_all_services()
        zeroconf.close()


