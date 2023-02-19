import os
import asyncio
import sys
import serial
import serial.tools.list_ports
import time
from libs.SerialProcess import SerialManager
from multiprocessing import Event
from libs.BLEConnection import Connection


##########################################################################################
# Bluetooth Configuration
##########################################################################################
# Device name
device_name = "DAQController-Core2"

# UUID creator
uuid = lambda x: f"4fafc201-{x}-459e-8fcc-c5c9c331914b"
# Server
service = uuid("0000")
# characteristics
read_characteristic = uuid("0001")
write_characteristic = uuid("0002")

# create the event loop
loop = asyncio.new_event_loop()

# The main bluetooth event
async def ble_main(connection, write_characteristic, run_event):
    running = False
    while True:
        if connection.client:
            # Look for the run event
            if run_event.is_set() and not running:
                bytes_to_send = "1".encode("utf-8")
                await connection.client.write_gatt_char(write_characteristic, bytes_to_send, response=True)  
                running = True

            if (not run_event.is_set()) and running:
                bytes_to_send = "0".encode("utf-8")
                await connection.client.write_gatt_char(write_characteristic, bytes_to_send, response=True)  
                running = False

        await asyncio.sleep(2)


if __name__ == '__main__':
    # Path to store data
    data_path = os.path.join(os.path.dirname(os.getcwd()), "data")
    if not os.path.exists(data_path):
        os.makedirs(data_path)
    print(data_path)
    
    # Set the file name
    file_name = "data-cobs"

    # Find the port for Nicla Sense ME
    nicla_port_info = None
    while True:
        ports = serial.tools.list_ports.grep("Nicla")
        for port in ports:
            if "Nicla Sense" in port.description:
                nicla_port_info = {
                    "device": port.device,
                    "name": port.name,
                    "description": port.description,
                    "hwid": port.hwid,
                    "location": port.location,
                }
                break
            else:
                pass
        
        if nicla_port_info is not None:
            break
        else:
            sys.stdout.write("Cannot find the device. Please check if the device is connected\n")
            time.sleep(10)

    # Print identified port
    sys.stdout.write(f"The port for {nicla_port_info['description']} is identified to be {nicla_port_info['device']}\n")
    # Serial params
    port = nicla_port_info["device"]
    baudrate = 460800

    ##########################################################################################
    # Processor
    ##########################################################################################
    # Events
    start_event = Event()
    run_event = Event()
    end_event = Event()
    # Process
    sm = SerialManager(serial_port=port, baudrate=baudrate, st_event=start_event, run_event=run_event, end_event=end_event, 
                        file_name=file_name, save_path=data_path)
    sm.start()

    # BLEConnection
    connection = Connection(
        loop=loop,
        read_characteristic=read_characteristic,
        write_characteristic=write_characteristic,
        device_name=device_name,
        st_event=start_event,
        ed_event=end_event
    )
    # Start the BLE operation
    try:
        main_task = asyncio.ensure_future(ble_main(connection, write_characteristic, run_event), loop=loop)
        ble_manager_task = asyncio.ensure_future(connection.manager(), loop=loop)
        loop.run_forever()
    except KeyboardInterrupt:
        print()
        print("Program has been interrupted")
    finally:
        # Disconnect
        print("Disconnecting")

        # Ensure Process end
        end_event.set()
        sm.terminate()
        sm.join()

        # Cancel all tasks 
        ble_manager_task.cancel()
        main_task.cancel()
        loop.run_until_complete(connection.cleanup())
        
