# arduino code:
"""
// incoming serial byte

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
  delay(1);
  if (Serial.available() > 0) {
    int v = Serial.read();  // can be -1 if read error
    if (v != 10) {
      Serial.println(v);
      activateRelay();
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
wait = 2
while True:
    ser = serial.Serial(port_name, 9600, write_timeout=0.5, timeout=0.5)
    time.sleep(wait)
    ser.write(b"1")
    if ser.readline() != b"":
        ser.close()
        break
    ser.close()
    wait += 0.5
while True:
    time.sleep(0.1)
    # check if drive is available called RPI-RP2
    if "RPI-RP2" in subprocess.check_output("df -H", shell=True).decode():
        break
