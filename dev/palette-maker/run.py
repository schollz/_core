# install Thonny to use:
# https://projects.raspberrypi.org/en/projects/getting-started-with-the-pico/2

from machine import Pin, SPI
import time
import neopixel


class MCP3208:
    def __init__(self, spi, cs_pin):
        self.spi = spi
        self.cs_pin = Pin(cs_pin, Pin.OUT)
        self.buffer = bytearray(3)
        self.data = bytearray(3)

    def read(self, channel, differential):
        if channel < 0 or channel > 7:
            raise ValueError("ADC channel must be between 0 and 7")

        val = 0
        self.buffer[0] = 0x01  # Start bit
        self.buffer[1] = ((1 << 7) | (channel << 4)) & 0xF0
        self.buffer[2] = 0

        self.cs_pin.value(0)  # Pull CS low to start the communication
        self.spi.write_readinto(self.buffer, self.data)
        self.cs_pin.value(1)  # Pull CS high to end the communication

        if len(self.data) == 3:
            val = 1023 - ((self.data[1] & 0x0F) << 8 | self.data[2])

        return val


def hue_to_rgb(hue):
    if hue < 85:
        r = hue * 3
        g = 255 - hue * 3
        b = 0
    elif hue < 170:
        hue -= 85
        r = 255 - hue * 3
        g = 0
        b = hue * 3
    else:
        hue -= 170
        r = 0
        g = hue * 3
        b = 255 - hue * 3

    return r, g, b


def show_color(r, g, b):
    for i in range(18):
        n[i] = (
            min(round(float(r) * brightness / 255.0), 255),
            min(round(float(g) * brightness / 255.0), 255),
            min(round(float(b) * brightness / 255.0), 255),
        )
    print("brightness=", brightness, "rgb=", n[0])
    n.write()


def show_hue(hue):
    for i in range(18):
        r, g, b = hue_to_rgb(hue)
        n[i] = (
            min(round(float(r) * brightness / 255.0), 255),
            min(round(float(g) * brightness / 255.0), 255),
            min(round(float(b) * brightness / 255.0), 255),
        )
    n.write()


# SPI configuration
CS_PIN = 9
SCK_PIN = 10
MOSI_PIN = 11
MISO_PIN = 8
KNOB_AMEN = 0
spi = SPI(
    1,
    baudrate=1000000,
    polarity=0,
    phase=0,
    sck=Pin(SCK_PIN),
    mosi=Pin(MOSI_PIN),
    miso=Pin(MISO_PIN),
)
cs = Pin(CS_PIN, Pin.OUT)
mcp3208 = MCP3208(spi, CS_PIN)

# Initialize WS2812
ws2812 = Pin(7, Pin.OUT)
n = neopixel.NeoPixel(ws2812, 18)

# tap tempo led for debugging
led = Pin(3, Pin.OUT)

# gamma of 2.7 to improve perceived brightness
brightness_values = [
    0,
    1,
    2,
    3,
    4,
    6,
    10,
    15,
    21,
    30,
    39,
    51,
    64,
    80,
    97,
    117,
    140,
    164,
    192,
    222,
    255,
]
KNOB_BREAK = 3
KNOB_AMEN = 0
KNOB_SAMPLE = 6
KNOB_BREAK_ATTEN = 4
KNOB_AMEN_ATTEN = 1

while True:
    adc_values = [mcp3208.read(channel, differential=False) for channel in range(8)]
    # print(" | ".join(f"Channel {i}: {value:4d}" for i, value in enumerate(adc_values)))
    led.toggle()
    brightness = brightness_values[
        int(round(float(adc_values[KNOB_BREAK_ATTEN]) * 20.0 / 1023.0))
    ]
    show_color(
        round(adc_values[KNOB_BREAK] / 4.0),
        round(adc_values[KNOB_AMEN] / 4.0),
        round(adc_values[KNOB_SAMPLE] / 4.0),
    )
    time.sleep(0.1)
