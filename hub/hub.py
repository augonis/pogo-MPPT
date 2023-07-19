import asyncio
import json
import socket
import curses
import struct
from curses import wrapper
from collections import namedtuple, defaultdict
from itertools import repeat

UDP_IP = "0.0.0.0"
UDP_PORT = 5005

def main(stdscr: curses.window):
    stdscr.clear()

    sock = socket.socket(socket.AF_INET, # Internet
                        socket.SOCK_DGRAM) # UDP

    sock.bind((UDP_IP, UDP_PORT))

    class Formatters:
        float = lambda f: ("%.4f" %f, 0)
        hex  = lambda x: ("%x" %x, 0)
        bool  = lambda b: (str(b), GREEN if b else RED)

    messageStruct = struct.Struct("L ffffffff fff I")
    fieldNames = ("MAC", "inputVoltage", "outputVoltage", "inputCurrent", "outputCurrent", "fetTemperature", "outputPower", "samplesPerSecond", "outputCharge", "minInputVoltage", "maxOutputVoltage", "maxOutputCurrent", "status")
    fieldFormats = ("%x", "%.4f", "%.4f", "%.4f", "%.4f", "%.4f", "%.4f", "%.3f", "%.4f", "%.4f", "%.4f", "%.4f", "%x")
    stateBitNames = ("STATE_SENSORS_READY", "STATE_SENSORS_READ_EVENT", "STATE_BUCK_MODE_CIV", "STATE_BUCK_MODE_COV", "STATE_BUCK_MODE_COC", 
                    "STATE_BUCK_MODE_SAFED", "STATE_BACKFLOW_ENABLED", "STATE_NETWORK_CONNECTED")

    numRows = len(fieldNames)+len(stateBitNames)
    headerWidth = len(max(fieldNames+stateBitNames, key=len))+2


    curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_RED)
    curses.init_pair(2, curses.COLOR_BLACK, curses.COLOR_GREEN)
    RED = curses.color_pair(1)
    GREEN = curses.color_pair(2)

    class HeaderFrame:
        @staticmethod
        def formatted():
            return zip(fieldNames[:-1]+stateBitNames, repeat(0))

    class TelemetryFrame(namedtuple('TelemetryFrame', fieldNames[:-1]+stateBitNames)):
        
        formatters = (
            Formatters.hex, Formatters.float, Formatters.float, Formatters.float, Formatters.float, Formatters.float, Formatters.float, Formatters.float, Formatters.float, Formatters.float, Formatters.float, Formatters.float,
            Formatters.bool, Formatters.bool, Formatters.bool, Formatters.bool, Formatters.bool, Formatters.bool, Formatters.bool, Formatters.bool, 
        )
        
        def __new__(cls, raw=None, **kwargs) -> None:
            if raw:
                unpacked = messageStruct.unpack(raw)
                state = unpacked[-1]
                return super().__new__(cls, *unpacked[:-1], *(bool(state & 1<<i) for i in range(len(stateBitNames))))
            return super().__new__(cls, **kwargs)
        
        def formatted(self):
            for f, v in zip(self.formatters, self):
                yield f(v)

    class Column:
        
        def __init__(self, width=16, height=numRows) -> None:
            self.width = width
            self.hwight = height
            self.window = curses.newwin(height, width, 2, sum((c.width+1 for c in columns.values()))+1)
        
        
        def update(self, frame):
            self.window.clear()
            for r, (t, attr) in enumerate(frame.formatted()):
                self.window.addstr(r, 0, t, attr)
            
            self.window.refresh()
    
    
    columns = defaultdict(Column)
    
    
    
    
    columns = defaultdict(Column)
    columns['header'] = Column(width=headerWidth)
    
    columns['header'].update(HeaderFrame)
    
    
    #write headers
    for r, n in enumerate(fieldNames[:-1], 2):
        stdscr.addstr(r, 1, n)
    for r, n in enumerate(stateBitNames, r+1):
        stdscr.addstr(r, 1, n)
    
    stdscr.refresh()
    
    while True:
        data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
        if len(data) > 100:
            kwargs = json.loads(data)
            kwargs.pop('timestamp', None)
            frame = TelemetryFrame(**kwargs)
        else:
            frame = TelemetryFrame(data)
        columns[frame.MAC].update(frame)
        
        continue
        
        unpacked = messageStruct.unpack(data)
        parsed = dict(zip(fieldNames, unpacked))
        state = parsed['status']
        state = {n: bool(state & 1<<i) for i, n in enumerate(stateBitNames)}
        
        for r, (f, v) in enumerate(zip(fieldFormats, unpacked),2):
            #print(headerWidth, r, (f % v).ljust(10))
            stdscr.addstr(r, headerWidth, (f % v).ljust(10))
        
        for r, v in enumerate(state.values(),r+1):
            stdscr.addstr(r, headerWidth, str(v).ljust(10), GREEN if v else RED)
        
        #print(parsed)
        stdscr.refresh()




if __name__ == '__main__':
    #sock.send('Hello ESP32'.encode())
    
    wrapper(main)
    