# arduino code:
"""
void setup() {
  Serial.begin(9600);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
}

void activateRelay() {
  digitalWrite(5, 1);
  delay(200);
  digitalWrite(4, 1);
  delay(200);
  digitalWrite(4, 0);
  delay(200);
  digitalWrite(5, 0);
}

void loop() {
  if (Serial.available() > 0) {
    int incomingData = Serial.read();  // can be -1 if read error
    switch (incomingData) {
      case '1':
        activateRelay();
        break;
      default:
        break;
    }
  }
}
"""

import time
import sys
import subprocess

import serial.tools.list_ports
import serial

ports = serial.tools.list_ports.comports()
port_name = ""
for port in ports:
    if "Arduino" in port.manufacturer:
        port_name = port.device
        break

if port_name == "":
    raise ("could not find Arduino")

print(f"pinging {port_name}")
ser = serial.Serial(port_name, 9600)
ser.write(b"1")
while True:
    time.sleep(0.1)
    # check if drive is available called RPI-RP2
    if "RPI-RP2" in subprocess.check_output("df -H", shell=True).decode():
        break

ser.close()
