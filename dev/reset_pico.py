# pip install pyserial

import serial
import sys
import time

ser = serial.Serial(sys.argv[1], 9600)
# clear serial data
ser.write(("\n\n\n").encode("utf-8"))
# turn off the buttons
ser.write(("20go\n").encode("utf-8"))
time.sleep(0.4)
ser.write(("30go\n").encode("utf-8"))
time.sleep(0.4)
# turn on the first button
print("BOOT ON")
ser.write(("21go\n").encode("utf-8"))
time.sleep(0.4)
# turn on the second button
print("RESET ON")
ser.write(("31go\n").encode("utf-8"))
time.sleep(0.4)
# turn off the second button
print("RESET OFF")
ser.write(("30go\n").encode("utf-8"))
time.sleep(0.4)
# turn off the first button
print("BOOT OFF")
ser.write(("20go\n").encode("utf-8"))
time.sleep(0.4)

ser.close()
