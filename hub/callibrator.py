from contextlib import suppress
import json
import socket
import threading
import asyncio
from zeroconf import ServiceBrowser, ServiceListener, Zeroconf, ServiceInfo
import curses
import struct
from display import StatusConsoleDisplay

## ask for device


async def deviceSelection(display):
    devices = []
    class Listener(ServiceListener):
        def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
            pass
        
        def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
            pass
        
        def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
            info = zc.get_service_info(type_, name)
            devices.append(info)
            niceName, _, _ = name.partition('.')
            display.print(f"\t {len(devices)} - {niceName}")
    
    zeroconf = Zeroconf()
    browser = ServiceBrowser(zeroconf, "_md-mppt-cal._udp.local.", Listener())
    
    
    display.print('Detected devices:')
    
    i: str = await display.input("Select device to collibrate:")
    zeroconf.close()
    return devices[int(i)-1]


class CallibratorClientProtocol (asyncio.DatagramProtocol):
    display: StatusConsoleDisplay
    
    def __init__(self, display):
        self.display = display
        self.on_con_lost = asyncio.get_running_loop().create_future()
        self.transport = None
        self.promptTask = None
    
    def connection_made(self, transport):
        self.transport = transport
        self.display.print(str(transport._extra['sockname']))
        
        #self.transport.sendto(b'Hello!')
    
    def datagram_received(self, data: bytes, addr):
        if data.startswith(b'ST:'):
            self.display.setStatus(data[3:].decode())
        elif data.startswith(b'PR:'):
            key = data[3:4]
            format, _, prompt = data[5:].partition(b':')
            self.promptTask and self.promptTask.cancel()
            self.promptTask = asyncio.create_task(self.prompt(key, format, prompt.decode()))
            
        else:
            self.display.print(data.decode())
        

    def error_received(self, exc):
        print('Error received:', exc)

    def connection_lost(self, exc):
        print("Connection closed")
        #self.on_con_lost.set_result(True)
    
    async def prompt(self, key, format, prompt):
        value = await self.display.input(prompt)
        
        if format==b'f': value=float(value)
        
        self.transport.sendto(b'RS:'+key+struct.pack(format, value))
        self.promptTask = None


async def main():
    with StatusConsoleDisplay() as display, suppress(KeyboardInterrupt):
        device = await deviceSelection(display)
        address =socket.inet_ntoa(device.addresses[0])
        display.print(f'Connecting to {device.name} ({address})')
        
        loop = asyncio.get_running_loop()
        
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(0)
        sock.sendto(b'Hello', (address, device.port))
        localAddr = sock.getsockname()
        sock.close()
        
        display.print(f'localaddr {localAddr}')
        transport, protocol = await loop.create_datagram_endpoint(
            lambda: CallibratorClientProtocol(display),
            local_addr=localAddr,
            remote_addr=(address, device.port),
            )
        
        
        
        try:
            await protocol.on_con_lost
        finally:
            transport.close()
            
        await display.input("Done, press enter to exit.")
        
    print('DONE')


asyncio.run(main())





