import glob
import os
from cobs import cobs
import struct
import argparse


if __name__ == '__main__':

    # Arguments to the program
    parser = argparse.ArgumentParser(description="Path to the data directory")
    parser.add_argument("-l", action="store", dest="data_directory", default="data", 
        help="Relative reference to the directory location for data", type=str)
    parser.add_argument("-nb", action="store", dest="num_bytes_read", default=11, 
        help="Number of bytes to unpack", type=int)
    args = parser.parse_args()

    # Ensure to get all ".dat" files
    data_files = sorted(glob.glob(args.data_directory + os.sep + "**.dat", recursive=True, root_dir=os.getcwd()))
    
    # Go through files one-by-one
    for data_file in data_files:
        # Location to save the processed file
        csv_save_path = os.path.join(os.sep.join(data_file.split(os.sep)[0:-1]), "processed_data")
        if not os.path.exists(csv_save_path):
            os.makedirs(csv_save_path)
        
        # Get the save file name
        csv_save_fname = os.path.join(csv_save_path, data_file.split(os.sep)[-1].split(".")[0] + ".csv")

        # Writer
        file_writer = open(csv_save_fname, "wt") 

        # Reader
        with open(data_file, "rb") as file_reader:
            # Read byte-by-byte
            data_byte = file_reader.read(1)
            # Concatenation
            temp_data_bytearray = b''

            while data_byte:
                # Look for the b'\x00'
                if data_byte != b'\x00':
                    temp_data_bytearray += data_byte
                    # Update byte reader
                    data_byte = file_reader.read(1)
                else:
                    decoded = cobs.decode(temp_data_bytearray)
                    # Unpack the data
                    unpacked_data = struct.unpack("f" * args.num_bytes_read, decoded)
                    # Print as a CSV line
                    print(",".join([str(x) for x in unpacked_data]), file=file_writer)

                    # Reset the byte array
                    temp_data_bytearray = b''
                    # Update byte reader
                    data_byte = file_reader.read(1)
        
        file_writer.flush()
        file_writer.close()

