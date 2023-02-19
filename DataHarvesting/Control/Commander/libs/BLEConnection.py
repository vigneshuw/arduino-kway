from bleak import BleakClient, BleakScanner
import asyncio
import sys
import struct


class Connection:

    client: BleakClient = None

    def __init__(self, 
    loop: asyncio.AbstractEventLoop,
    read_characteristic: str,
    write_characteristic: str,
    device_name: str, 
    st_event, 
    ed_event) -> None:
        
        # Initialize
        self.loop = loop
        self.read_characteristic = read_characteristic
        self.write_characteristic = write_characteristic
        self.device_name = device_name
        # Event for process control
        self.st_event = st_event
        self.ed_event = ed_event

        # Device state
        self.connected = False
        self.connected_device = None

        # Disconnect counter
        self.disconnect_counter = 0

    def on_disconnect(self, client):
        self.connected = False
        # Stop the DAQ process
        self.st_event.clear()
        # Clear the client
        self.client = None
        sys.stdout.write(f"Disconnected from {self.connected_device.name}")

    def notification_handler(self, sender:str, data:bytearray):
        self.daqControl = struct.unpack("<B", data)[0]
        sys.stdout.write(f"Control - {self.daqControl}\n")

        # Decision based on DAQ Control
        if self.daqControl:
            self.st_event.set()
        else:
            self.st_event.clear()

    async def manager(self):
        sys.stdout.write("Starting connection manager\n")
        while True:
            if self.client:
                await self.connect()
            else:
                await self.select_device()
                await asyncio.sleep(2.0)

    async def connect(self):
        # If already connected
        if self.connected:
            return 

        # try to connect 
        try:
            await self.client.connect()
            self.connected = self.client.is_connected
            if self.connected:
                sys.stdout.write(f"Connected to {self.connected_device.name}\n")

                # Notification handlers
                await self.client.start_notify(
                    self.read_characteristic, self.notification_handler
                )

                while True:
                    if not self.connected:
                        break
                    await asyncio.sleep(15.0)
            else:
                sys.stdout.write(f"Failed to connect {self.connected_device.name}\n")
        except Exception as e:
            print(e)

    async def cleanup(self):
        # Disconnect BLE
        if self.client:
            await self.client.disconnect()

        # Stop running loops 
        self.loop.stop()

    async def select_device(self):
        sys.stdout.write("Bluetooth LE hardware warming up....\n")
        devices = await BleakScanner.discover()

        # make connection from selection
        sys.stdout.write(f"Connecting to {self.device_name}\n")
        response = [x for x in devices if x.name == self.device_name]
        if response:
            self.connected_device = response[0]
            self.client = BleakClient(response[0].address, disconnected_callback=self.on_disconnect, loop=self.loop)
        
        await asyncio.sleep(5.0)

