#!/usr/bin/env python3
#
# This scripts is the companion application for the DataHarvester sketch.
# It is a very basic application and can be improved and extended.
#
# It waits for user input to control the data acquisition operations and the
# connection to the Arduino board and to save the received data to a CSV file.
#
# It establish a serial communication to the board and uses COBS as enconding and
# reliable transport protocol.
#
# The main thread will always wait for user input while a second one is created
# to exchange both control and data with the Arduino board.
# The two threads coordinate via a simple Queue.

# Thread and Queue
from threading import Thread
import queue

# Helpers
from datetime import datetime
import os
import sys

# Communication with the Arduino board
import serial
import serial.tools.list_ports
from cobs import cobs


# The main serial object
# Initialise later
ser = serial.Serial()

# The function to be run in the Arduino-facing thread
def receiver(cmd_q, uart, name, save_path):
    f = None
    running = False
    cmd = 'N'

    while True:
        # Check for commands from the main thread
        try:
            cmd = cmd_q.get(block=False)
        except queue.Empty:
            pass

        # Start acquisition, prepare output file, etc.
        if cmd in 'AGR':
            running = True
            today = datetime.today()
            iso = datetime.isoformat(today, timespec="seconds")
            filename = f'{name}-{cmd}_{iso}.dat'
            file_full_path = os.path.join(save_path, filename)
            f = open(file_full_path, 'wb')
            cmd = 'N'
        # Quit program
        elif cmd == 'Q':
            running = False
            if f != None:
                if not f.closed:
                    f.flush()
                    f.close()
            break

        # Receive data packet-by-packet and save to file
        if running:
            if uart.is_open:
                # Read full COBS packet
                data = uart.read_until(b'\x00')
                f.write(data)


# The main thread
if __name__ == '__main__':

    # Path to store data
    data_path = os.path.join(os.path.dirname(os.getcwd()), "data")
    if not os.path.exists(data_path):
        os.makedirs(data_path)
    
    # Set the file name
    file_name = "data-cobs"

    # Find the port for Nicla Sense ME
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
    # Print identified port=
    sys.stdout.write(f"The port for {nicla_port_info['description']} is identified to be {nicla_port_info['device']}\n")

    # Set pyserial params
    ser.port = nicla_port_info["device"]
    # Baudrate depends on the sensor active
    # Only one sensor -> 250000 at 800 Hz; Can be increase to 921600
    # All sensors -> 460800 at 500 Hz; Cannot go more than this
    ser.baudrate = 250000
    ser.close()
    ser.open()

    # Inter-thread communication queue
    q = queue.Queue()

    # Arduino-facing thread
    recv_thread = Thread(target=receiver, args=(q, ser, file_name, data_path))
    recv_thread.start()

    # Wait for user input
    s = "S"
    while True:
        # Use input dependence on the stage of the program
        if s == "S":
            s = input("Command the DAQ process (AGRQ) > ").upper()
        else:
            qs = input("Quit Running Program (Q) > ").upper()
            if qs != "Q":
                print("You can only quit the program at this point. Please check input!")
                continue
            else:
                s = "Q"

        # Send commands to receiver thread
        if s == "A":
            print("Collecting acceleration data.....")
            q.put("A")
        elif s == "G":
            print("Collecting gyroscope data....")
            q.put("G")
        elif s == 'R':
            print('Collecting quaternion data...')
            q.put('R')
        elif s == 'Q':
            print('Quitting...')
            q.put('Q')
            recv_thread.join()
        else:
            print("Incorrect choice, please select again!")
            continue

        # Send commands to the Arduino board
        if ser.is_open:
            enc = cobs.encode(s.encode())
            ser.write(enc)
            ser.write(b'\x00')
            if s == "Q":
                if ser.is_open:
                    ser.close()
                    break
