from multiprocessing import Process
from datetime import datetime
import serial
import os
import sys
from cobs import cobs


class SerialManager(Process):

    def __init__(self, serial_port, baudrate, st_event, run_event, end_event, file_name, save_path) -> None:
        super(SerialManager, self).__init__(target=self.receiver, args=(st_event, run_event, end_event, serial_port, baudrate, file_name, save_path))

    def receiver(self, st_event, run_event, end_event, serial_port, baudrate, file_name, save_path):
        """
        Run for every new DAQ
        """
        # Initialize the serial object
        ser = serial.Serial()
        ser.port = serial_port
        ser.baudrate = baudrate
        sys.stdout.write(f"Serial Connection has been initialized\n")

        # wait for the first run event
        st_event.wait()

        running = False
        while True:

            # Check for commands from the main thread
            if st_event.is_set() and not running:
                # Try to see if serial can open
                try:
                    ser.open()
                except serial.SerialException as e:
                    continue

                running = True
                # Create unique file
                today = datetime.today()
                iso = datetime.isoformat(today, timespec="seconds")
                filename = f'{file_name}_{iso}.dat'
                file_full_path = os.path.join(save_path, filename)
                f = open(file_full_path, 'wb')

                # Transfer the initialization byte
                if ser.is_open:
                    enc = cobs.encode("R".encode())
                    ser.write(enc)
                    ser.write(b'\x00')
                # If DAQ works
                if ser.is_open:
                    run_event.set()

            if not st_event.is_set() and running:
                # Update running status
                running = False
                if f != None:
                    if not f.closed:
                        f.flush()
                        f.close()

                # Stop the DAQ
                # Transfer the initialization byte
                if ser.is_open:
                    enc = cobs.encode("Q".encode())
                    ser.write(enc)
                    ser.write(b'\x00')
                # Finish up
                ser.close()
                run_event.clear()

            if (not st_event.is_set()) and end_event.is_set():
                # Ensure that the file and serial is closed
                if f != None:
                    if not f.closed:
                        f.flush()
                        f.close()
                # Stop the DAQ
                # Transfer the initialization byte
                if ser.is_open:
                    enc = cobs.encode("Q".encode())
                    ser.write(enc)
                    ser.write(b'\x00')
                    # Close serial
                    ser.close()
                run_event.clear()
                print("Stopping the Process Execution")
                break

            # Receive data packet-by-packet and save to file
            if running:
                if ser.is_open:
                    try:
                        # Read full COBS packet
                        data = ser.read_until(b'\x00')
                        f.write(data)
                    except serial.SerialException as e:
                        # Close the port
                        ser.close()
                        # Close the current data file
                        f.flush()
                        f.close()
                        # log the file - as it is corrupted
                        with open(os.path.join(save_path, "error_log.txt"), "at") as error_fhandle:
                            error_fhandle.write(f"{filename}\n")
                        st_event.clear()
                        run_event.clear()
                        running = False
                    
