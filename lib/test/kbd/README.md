# pico\_usb\_kbd\_lcd 

Kevin Boone, November 2022

## What is this?

A program for the Pico, to test USB keyboard input using an I2C LCD display.

pico\_usb\_kbd\_lcd is a program for the Raspberry Pi Pico microcontroller,
that reads a USB keyboard, and echos the characters to an alphanumeric LCD
matrix display using I2C. I2C LCD modules are inexpensive and widely available,
on eBay and elsewhere.  These modules seem to be very similar: they typically
use the HD44780 display module, driven by a PCF8574 shift register. All these
units, I suspect, come from the same factory, as they work in a similar way.
This application works with displays of up to 40x4 characters, although units
as large as this do not seem to be widely available.

This program demonstrates how to use the Pico's USB host support, and how to
drive an alphanumeric LCD display using I2C.

For a detailed description of the protocol used by the HD44780, and the
complexities involved in driving one using I2C, see this article on my website:

https://kevinboone.me/pi-lcd.html

## Wiring 

For USB host support you will need an adapter cable, from the micro-USB of the
Pico to USB type-A female. The keyboard will plug into the USB-A socket. The
cables are available powered and unpowered. The powered versions are sometimes
called "OTG cables" or "USB Y cables". 

To use an unpowered adapter, you'll need to power the Pico in some way other
than via USB.

The wiring of the LCD module that corresponds to the program's defaults is as
follows:

    Pico            I2C LCD
    ----            --------
    Pin 6 (SDA0)    Data / SDA
    Pin 7 (SCL0)    Clock / SCL
    Pin 40 (VBUS)   Vcc / +
    Pin 38 (GND)    Gnd / -

It's probably possible to power the LCD module from the Pi's 3.3V line on pin
36, although these modules are designed to work with a 5V supply.  Using the
3.3V supply -- if it works -- will consume less energy, because the display
backlight will not be as bright. 

Please note that the LCD module probably has a contrast adjustment, and it
probably will need to be adjusted to get the best appearance.

## Building

You'll need `cmake` and the Pi Pico SDK for C. Please review the hardware
settings in `config.h` before building.

    $ export PICO_SDK_PATH=/path/to/pico/sdk 
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make

This produces a `UF2` files that can be copied to the Pico when it is in
bootloader mode.

## Running 

The program displays "Hello" on the LCD on power-up, to prove that the display
is working. Thereafter it echos keystrokes from a USB keyboard. The keyboard
can be attached before or after the Pico boots.

## Configuration 

Some important configuration settings are in `config.h`, including the size and
I2C address of the I2C LCD module. Most I2C LCD modules are factory-set to
address 0x27 -- this is the address we get if the address-setting pins on the
PCF8574 chip are left completely unconnected, which is the cheapest
configuration for the manufacturer. 

## Directories

`i2c_lcd`: driver and terminal-like handler for I2C LCD displays based
on the HD44780 and PCF8574. 

`usb_kbd`: a driver for USB HID keyboards. 

`kbd`: general keyboard utility functions, such as handling of modifier
keys, which are not specific to a particular type of keyboard.

## Limitations

- It should be obvious that the Pico only has one USB port. It can
  be used only in host mode or device mode at a given time. You can't
  connect a serial terminal to the USB port for debugging, or programming,
  when it's connected to a USB keyboard. 

- Only US keyboard layouts are supported. Other layouts should work to
  some extent but, for proper support, keyboard mapping tables would be
  needed for different layouts. 

- There have been reports that certain wireless USB keyboards don't work,
  for reasons that are unclear at present.  

- LCD modules of the HD44780 type are essentially ASCII devices. Although
  they do have an 8-bit character set, it doesn't match any particular
  encoding. This isn't a problem when the data source is a US-layout
  keyboard. 

- Using an I2C interface to operate the HD44780 display, particularly in
  4-bit mode, is slow. At a safe I2C speed of 100,000 baud, about 3000
  characters can be sent per second. However, because I2C LCD displays
  are usually small-ish, this should not create too much of a problem.

- For reasons that I cannot even begin to fathom, the Pico USB host 
  support does not work if the Pico SDK is set to build C code with
  lots of GCC warnings enabled. I usually like to enable "-Wall" at least,
  but this causes the program to fail. I've tested this many times and,
  although it seems incomprehensible, it's 100% repeatable. 

## Notes

- The application should respond to keyboard control characters, like
  enter, backspace (which deletes), and form-feed (ctrl-L, which erases
  the display).

- The up and down arrow keys scroll back through previous lines that
  have been scrolled off the top of the display. Entering any other
  character should reset the display to its original position.
   
- The program has been tested using version 1.4.0 of the Pi Pico C SDK.
  It's probable that earlier versions will not work. 

