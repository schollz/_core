/*============================================================================
 *  i2c_lcd/i2c_lcd.h
 *
 *  Interface functions for the I2C LCD module. 
 *
 *  For a description of the protocol, please see
 *  https://kevinboone.me/pi-lcd.html
 *
 * Copyright (c)2022 Kevin Boone, GPL v3.0
 * ==========================================================================*/

#pragma once

#include <hardware/i2c.h>

#ifndef BOOL
typedef int BOOL;
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE 
#define FALSE 0
#endif

/*============================================================================
 * HD44780 pin assignments. Rather than using pin numbers (0, 1, 2) we
 * use the hex equivalents, just to avoid additional computation. 
 * RS - high to indicate that a character is being send, low for a command
 * RW - read/wriate
 * Enable - this is the clock pin, that is set high to shift in data
 * Backlight -- connected to the LED backlight control
 * ==========================================================================*/

// Setting 0x01 for RS assumes that the RS pin on the HD44780 is
//   connected to pin 1 on the I2C controller. This is a common choice, but
//   not universal.
#define I2C_LCD_RS 0x01

// Setting 0x02 for RW assumes that the RS pin on the HD44780 is
//   connected to pin 1 on the I2C controller. This is a common choice, but
//   not universal.
// Note that this pin is not used in the current application.
#define I2C_LCD_RW 0x02

// Setting 0x04 for enable assumes that the enable pin on the HD44780 is
//   connected to pin 3 on the I2C controller. This is a common choice, but
//   not universal.
#define I2C_LCD_ENABLE 0x04

// Setting 0x08 for backlight assumes that the backlight control pin on 
//   the HD44780 is connected to pin 4 on the I2C controller. This is a 
//   common choice, but not universal.
#define I2C_LCD_BACKLIGHT 0x08

// Length of time in microseconds to pulse the enable line. In reality the
//   time will be a bit longer, because of the I2C protocol overhead
#define I2C_LCD_DELAY 600

typedef struct _I2C_LCD I2C_LCD;

#ifdef __cplusplus
extern "C" {
#endif

extern I2C_LCD *i2c_lcd_new (int width, int height, int addr, i2c_inst_t *i2c, 
                              int sda, int scl, int i2c_baud, 
                              int scrollback_pages);
extern void     i2c_lcd_destroy (I2C_LCD* self);

extern void     i2c_lcd_display_on (I2C_LCD* self);
extern void     i2c_lcd_display_off (I2C_LCD* self);

extern void     i2c_lcd_backlight_on (I2C_LCD* self);
extern void     i2c_lcd_backlight_off (I2C_LCD* self);

extern void     i2c_lcd_set_cursor (I2C_LCD *self, int row, int col);
extern void     i2c_lcd_print_string (I2C_LCD *self, const char *s);
extern void     i2c_lcd_print_char (I2C_LCD *self, const char c);

/** Move cursor down and to the start of the line. */ 
extern void     i2c_lcd_new_line (I2C_LCD *self);

/** Move the cursor down. If wrapping is enabled, and the cursor is
    on the bottom line, scroll up */
extern void     i2c_lcd_line_feed (I2C_LCD *self);

/** Show the cursor. */
extern void     i2c_lcd_cursor_on (I2C_LCD *self);
/** Hide the cursor. */
extern void     i2c_lcd_cursor_off (I2C_LCD *self);

/** Turn on line-wrapping and scrolling. */
extern void     i2c_lcd_wrapping_on (I2C_LCD *self);
/** Turn off line-wrapping and scrolling. */
extern void     i2c_lcd_wrapping_off (I2C_LCD *self);

/** Clears the display and sets the cursor to the top left */
extern void     i2c_lcd_clear (I2C_LCD *self, BOOL clear_scrollback);

/** CR expands to CRLF. */
extern void     i2c_lcd_implicit_lf_on (I2C_LCD *self);
/** CR is not expanded to CRLF. */
extern void     i2c_lcd_implicit_lf_off (I2C_LCD *self);

extern void     i2c_lcd_destructive_backspace_on (I2C_LCD *self);
extern void     i2c_lcd_destructive_backspace_off (I2C_LCD *self);

/* Non-destructive backspace -- just moves cursor. */
extern void     i2c_lcd_backspace (I2C_LCD *self);
/* Destructive backspace -- moves cursor and overwrites with space. */
extern void     i2c_lcd_del (I2C_LCD *self);

extern void i2c_lcd_scrollback_line_up (I2C_LCD *self);
extern void i2c_lcd_scrollback_line_down (I2C_LCD *self);

#ifdef __cplusplus
}
#endif



