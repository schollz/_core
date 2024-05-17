# usb\_kbd

This is a driver for a USB keyboard, using the Pico's USB interface
in host mode. 

# Usage

    void kbd_raw_key_down (int code, int flags)
      {
      // Handle keyboard character
      // ...
      }

    void main_loop()
      {
      usb_kbd_init();

      while (1)
        {
        usb_kbd_scan();
        // ...
        }
      }

The client program needs to call `usb_kbd_scan` at regular intervals,
much shorter than the time between keystrokes. Keycodes are delivered
to `kbd_raw_key_down()`, which the client program must supply. 

## Limitations

- Only US keyboard layouts are supported. Other layouts should work to
  some extent but, for proper support, keyboard mapping tables would be
  needed for different layouts. 

- There have been reports that certain wireles USB keyboards don't work,
  for reasons that are unclear at present.  

## Notes

The files `tusb_config.h` contains important configuration for the 
TinyUSB library. It is in the `include` directory, not `include/usb_kbd/`
because one of the TinyUSB header files includes this file using
the bare filename. The CMake build script has to have include directories
configured to find the file.

