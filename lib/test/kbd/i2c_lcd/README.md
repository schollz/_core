# I2C\_LCD

A driver for HD44870 displays with PCF8574 I2C interface, with some terminal 
features.

Although the eight output lines of the
PCF8574 (I2C shift register) can be wired to the eight input lines of
the HD44789 (LCD modules) in a huge number of different ways, in 
practice all the devices for sale appear to be wired similarly.  

## Usage

    I2C_LCD *i2c_lcd = i2c_lcd_new (16, 2, 0x27, PICO_DEFAULT_I2C_INSTANCE,
       PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, 100000);

    i2c_write_char (i2c_lcd, '!');
    i2c_write_string (i2c_lcd, "Hello, world");
    ...

The client application will need to specify the size of the display,
its I2C address, and the I2C pins on the Pico to which the I2C 
interface is connected. There might be some mileage in experimenting
with the baud rate. 

## Start-up state

Screen blank, cursor at top left, cursor visible, backlight on, backspace
is destructive, implict line-feed (CR -> CRLF) enabled, wrapping (terminal
mode) enabled.

## Terminal features

If wrapping is on, then the LCD display will behave like a terminal.
When text reaches the end of the current line, it will move to the next
line. When it reaches the bottom line, it will scroll up. The following
control characters are recognized:

Backspace (8): moves the cursor one place left. If destructive backspace
is enabled, behaves like Del (127).

Line feed (10): moves the cursor down on line, keeping the same column. If
the cursor is on the bottom line, scroll up.

Carriage return (13): moves the cursor to the start of the line, without
changing the line (unless implicit line feed is enabled).

Form feed (12): clear the display and set the cursor to the top left.

Del (127): destructive backspace

## Scrollback

Characters written to the display are stored in a buffer so that, when
the display scrolls up, previous lines are retained. The size of the
scrollback buffer is set when initializing the `I2C_LCD` object. 
For example, `i2c_lcd_scrollback_line()` moves back one line. In the
present implementation, it is possible to scroll back to an unused
part of the buffer. 

Whenever a new character is written, scrollback is cleared, and
the original display restored.

## Notes and limitations

The display is effectively ASCII-only. HD44870 devices have an 
8-bit character set, but this does not map to any standard encoding, and
varies from one model to another.

The way this code is organized, it would be difficult to attach anything
else to the I2C bus other than the LCD module. However, the Pico has two
I2C busses. 

`i2c_lcd.c` starts with a heap of definitions that relate to the HD44780
protocol. None of these should need to be changed, even if the hardware
is wired differently. `i2c_lcd.h` contains definitions that might, perhaps,
need to be changed -- notably the clock pulse width and the connections
between the PCF8547 and HD44870.


