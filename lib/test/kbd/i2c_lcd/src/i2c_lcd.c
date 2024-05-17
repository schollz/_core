/*============================================================================
 *  i2c_lcd/i2c_lcd.c
 *
 *  For a description of the protocol, please see
 *  https://kevinboone.me/pi-lcd.html
 *
 * Copyright (c)2022 Kevin Boone, GPL v3.0
 * ==========================================================================*/

#include <stdlib.h>
#include <string.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <i2c_lcd/i2c_lcd.h>

// I don't think these LCD panels were ever made with more than 4 rows
#define I2C_LCD_MAX_ROWS 4

#define I2C_LCD_ENTRY_RIGHT 0x00
#define I2C_LCD_ENTRY_LEFT 0x02

#define I2C_LCD_ENTRY_SHIFT_DECREMENT 0x00
#define I2C_LCD_ENTRY_SHIFT_INCREMENT 0x01

// I2C-LCD devices invariably use 4-bit transfers
#define I2C_LCD_MODE_4_BIT 0x00
#define I2C_LCD_MODE_8_BIT 0x10

#define I2C_LCD_LINE_1 0x00
#define I2C_LCD_LINE_2 0x08

// In practice, all the devices I've seen support only 5x8 mode
#define I2C_LCD_DOTS_5X10 0x04
#define I2C_LCD_DOTS_5X8 0x00

#define I2C_LCD_DISPLAY_OFF 0x00
#define I2C_LCD_DISPLAY_ON 0x04
#define I2C_LCD_CURSOR_OFF 0x00
#define I2C_LCD_CURSOR_ON 0x02
#define I2C_LCD_BLINK_OFF 0x00
#define I2C_LCD_BLINK_ON 0x01

#define I2C_LCD_COMMAND 0x00

// HD44780 LCD commands

#define I2C_LCD_CLEAR_DISPLAY 0x01
#define I2C_LCD_HOME 0x02
#define I2C_LCD_ENTRY_MODE_SET 0x04
#define I2C_LCD_DISPLAY_CONTROL 0x08
#define I2C_LCD_CURSOR_SHIFT 0x10
#define I2C_LCD_FUNCTION_SET 0x20
#define I2C_LCD_SET_CGRAM_ADDR 0x40
#define I2C_LCD_SET_DDRAM_ADDR 0x80


struct _I2C_LCD 
  {
  int width;
  int height;
  int addr;
  i2c_inst_t *i2c;  
  int curr_row;
  int curr_col;
  int scrollback_max_lines;
  int scrollback;
  unsigned char display_mode;
  unsigned char display_function;
  unsigned char display_control;
  unsigned short backlight;
  unsigned char offsets[I2C_LCD_MAX_ROWS];
  BOOL wrap;
  BOOL destructive_backspace;
  BOOL implicit_lf; 
  unsigned char *scrollback_buffer;
  };

//#define MIN(x,y) (x < y ? x : y)

/*============================================================================
 * i2c_write_byte 
 * ==========================================================================*/
static void i2c_write_byte (const I2C_LCD *self, unsigned char b)
  {
  unsigned char data = b | self->backlight;
  i2c_write_blocking (self->i2c, self->addr, &data, 1, false);
  }

/*============================================================================
 * pulse_enable_line 
 * ==========================================================================*/
static void pulse_enable_line (const I2C_LCD *self, unsigned char b)
  {
  sleep_us (I2C_LCD_DELAY);
  i2c_write_byte (self, b | I2C_LCD_ENABLE);
  sleep_us (I2C_LCD_DELAY);
  i2c_write_byte (self, b & ~I2C_LCD_ENABLE);
  sleep_us (I2C_LCD_DELAY);
  }

/*============================================================================
 * send_4bits
 * ==========================================================================*/
static void send_4bits (const I2C_LCD *self, unsigned char b)
  {
  i2c_write_byte (self, b);
  pulse_enable_line (self, b);
  }

/*============================================================================
 * send_byte
 * ==========================================================================*/
static void send_byte (const I2C_LCD *self, unsigned char b, unsigned char mode)
  {
  send_4bits (self, (b & 0xF0) | mode);
  send_4bits (self, ((b << 4) & 0xF0) | mode);
  }

/*============================================================================
 * send_command 
 * ==========================================================================*/
static void send_command (const I2C_LCD *self, unsigned char c)
  {
  send_byte (self, c, I2C_LCD_COMMAND);
  }

/*============================================================================
 * send_char
 * ==========================================================================*/
static void send_char (const I2C_LCD *self, char c)
  {
  send_byte (self, c, I2C_LCD_RS);
  }

/*============================================================================
 * reset_scrollback 
 * ==========================================================================*/
static void reset_scrollback (I2C_LCD *self)
  {
  memset (self->scrollback_buffer, ' ', 
    self->scrollback_max_lines * self->width);
  self->scrollback = 0;
  }

/*============================================================================
 * dump_scrollback 
 * Dump the scrollback buffer to the display, starting at the position of
 * self->scrollback. 
 * ==========================================================================*/
static void dump_scrollback (I2C_LCD *self)
  {
  int old_curr_row = self->curr_row;
  int old_curr_col = self->curr_col;

  int scrollback_start_line = self->scrollback_max_lines - self->height 
    - self->scrollback;

  for (int i = 0; i < self->height; i++)
    {
    int offset = (scrollback_start_line + i) * self->width;
    i2c_lcd_set_cursor (self, i, 0);
    for (int j = 0; j < self->width; j++)
      {
      send_char (self, self->scrollback_buffer[offset + j]);
      }
    }
  
  self->curr_row = old_curr_row;
  self->curr_col = old_curr_col;
  }

/*============================================================================
 *  cancel_scrollback
 *  Reset the display to the bottom of the scrollback buffer, that is, to
 *    the display that would have been visible before scrolling back. Only
 *    do this if, in fact, a scrollback is in force -- it causes an ugly
 *    flicker otherwise.
 * ==========================================================================*/
static void cancel_scrollback (I2C_LCD *self)
  {
  if (self->scrollback > 0)
    {
    self->scrollback = 0;
    dump_scrollback (self);
    i2c_lcd_set_cursor (self, self->curr_row, self->curr_col);
    }
  }


/*============================================================================
 * scroll_up 
 * ==========================================================================*/
static void scroll_up (I2C_LCD *self)
  {
  int orig_col = self->curr_col;

  // Shift scrollback buffer up one line

  memmove (self->scrollback_buffer, self->scrollback_buffer + self->width,
             self->width * (self->scrollback_max_lines - 1));
  memset (self->scrollback_buffer + (self->scrollback_max_lines - 1) 
            * self->width, ' ', self->width);
  
  // clear display

  i2c_lcd_clear (self, FALSE);

  // Dump bottom (height - 1) lines from the scrollback buffer to the display
  
  for (int i = 0; i < self->height - 1; i++)
    {
    int offset = (self->scrollback_max_lines - self->height + i + 0) 
      * self->width;
    i2c_lcd_set_cursor (self, i, 0);
    for (int j = 0; j < self->width; j++)
      {
      send_char (self, self->scrollback_buffer[offset + j]);
      }
    }

  // Set to original_column 
  i2c_lcd_set_cursor (self, self->height - 1, orig_col); 
  }

/*============================================================================
 *  i2c_lcd_new 
 * ==========================================================================*/
I2C_LCD *i2c_lcd_new (int width, int height, int addr, i2c_inst_t *i2c, 
                       int sda, int scl, int i2c_baud, int scrollback_pages)
  {
  I2C_LCD *self = malloc (sizeof (I2C_LCD));
  self->width = width;
  self->height = height;
  self->addr = addr;
  self->i2c = i2c;
  self->wrap = TRUE; 
  self->implicit_lf = TRUE;
  self->destructive_backspace = TRUE; 
  self->scrollback_max_lines = scrollback_pages * self->height; 
  self->scrollback_buffer = malloc (self->width * self->scrollback_max_lines);
  reset_scrollback (self);

  i2c_init (i2c, i2c_baud);
  gpio_set_function (sda, GPIO_FUNC_I2C);
  gpio_set_function (scl, GPIO_FUNC_I2C);
  gpio_pull_up (sda);
  gpio_pull_up (scl);
  self->backlight = 0;
  self->display_mode = I2C_LCD_ENTRY_LEFT | I2C_LCD_ENTRY_SHIFT_DECREMENT;
  self->display_function 
                    = I2C_LCD_MODE_4_BIT | I2C_LCD_LINE_2 | I2C_LCD_DOTS_5X8;
  self->display_control 
                    = I2C_LCD_DISPLAY_ON | I2C_LCD_CURSOR_ON | I2C_LCD_BLINK_OFF;

  self->offsets[0] = 0;
  self->offsets[1] = 0x40;
  self->offsets[2] = width;
  self->offsets[3] = 0x40 + width;
  
  self->curr_row = 0;
  self->curr_col = 0;
  
  // Basic init sequence. It's an ugly workaround for the fact that we
  //   don't know whether the unit starts up in 4-bit or 8-bit mode.
  send_command (self, 0x03);
  send_command (self, 0x03);
  send_command (self, 0x03);
  send_command (self, 0x02);

  send_command (self, I2C_LCD_ENTRY_MODE_SET | self->display_mode);
  send_command (self, I2C_LCD_FUNCTION_SET | self->display_function);

  i2c_lcd_display_on (self);

  // We might as well start with the backlight on -- this is the usual
  //   power-on state of these I2C LCD devices. The application call 
  //   always turn it off with i2c_lcd_backlight_off() later if necessary.
  i2c_lcd_backlight_on (self);

  return self;
  }

/*============================================================================
 *  i2c_lcd_display_on
 * ==========================================================================*/
void i2c_lcd_display_on (I2C_LCD* self)
  {
  self->display_control |= I2C_LCD_DISPLAY_ON;
  send_command (self, I2C_LCD_DISPLAY_CONTROL | self->display_control);
  }

/*============================================================================
 *  i2c_lcd_display_off
 * ==========================================================================*/
void i2c_lcd_display_off (I2C_LCD* self)
  {
  self->display_control &= ~I2C_LCD_DISPLAY_ON;
  send_command (self, I2C_LCD_DISPLAY_CONTROL | self->display_control);
  }

/*============================================================================
 *  i2c_lcd_backlight_on
 * ==========================================================================*/
void i2c_lcd_backlight_on (I2C_LCD* self)
  {
  self->backlight = I2C_LCD_BACKLIGHT;
  i2c_write_byte (self, self->backlight); 
  }

/*============================================================================
 *  i2c_lcd_backlight_off
 * ==========================================================================*/
void i2c_lcd_backlight_off (I2C_LCD* self)
  {
  self->backlight = 0;
  i2c_write_byte (self, self->backlight); 
  }

/*============================================================================
 *  i2c_lcd_set_cursor
 * ==========================================================================*/
void i2c_lcd_set_cursor (I2C_LCD *self, int row, int col)
  {
  // TODO -- should we constrain the column as well?
  row = MIN (row, self->height - 1);
  self->curr_row = row;
  self->curr_col = col;
  send_command (self, I2C_LCD_SET_DDRAM_ADDR | (self->offsets[row] + col));
  }

/*============================================================================
 *  i2c_lcd_print_string
 * ==========================================================================*/
void i2c_lcd_print_string (I2C_LCD *self, const char *s) 
  {
  while (*s)
    {
    i2c_lcd_print_char (self, *s);
    s++;
    }
  }

/*============================================================================
 *  i2c_lcd_line_feed
 * ==========================================================================*/
void i2c_lcd_line_feed (I2C_LCD *self) 
  {
  self->curr_row++;
  if (self->curr_row >= self->height)
    scroll_up (self);
  i2c_lcd_set_cursor (self, self->curr_row, self->curr_col);
  }

/*============================================================================
 *  i2c_lcd_cr
 * ==========================================================================*/
void i2c_lcd_cr (I2C_LCD *self) 
  {
  i2c_lcd_set_cursor (self, self->curr_row, 0);
  self->curr_col = 0;
  }

/*============================================================================
 *  i2c_cursor_on
 * ==========================================================================*/
void i2c_lcd_cursor_on (I2C_LCD *self) 
  {
  self->display_control |= I2C_LCD_CURSOR_ON;
  send_command (self, I2C_LCD_DISPLAY_CONTROL | self->display_control);
  }

/*============================================================================
 *  i2c_cursor_off
 * ==========================================================================*/
void i2c_lcd_cursor_off (I2C_LCD *self) 
  {
  self->display_control &= ~I2C_LCD_CURSOR_ON;
  send_command (self, I2C_LCD_DISPLAY_CONTROL | self->display_control);
  }

/*============================================================================
 *  i2c_lcd_backspace
 * ==========================================================================*/
void i2c_lcd_backspace (I2C_LCD *self) 
  {
  if (self->curr_col > 0)
    {
    self->curr_col--;
    i2c_lcd_set_cursor (self, self->curr_row, self->curr_col);
    }
  else
    {
    // If the cursor is not on the top line, backspace on the previous
    //   line, and set the cursor to the end of the line. I'm not sure
    //   whether this is the appropriate action to take -- what do
    //   real terminals do?
    if (self->curr_row > 0)
      {
      i2c_lcd_set_cursor (self, self->curr_row - 1, self->width - 1);
      }
    }
  }

/*============================================================================
 *  i2c_lcd_del
 * ==========================================================================*/
void i2c_lcd_del (I2C_LCD *self) 
  {
  i2c_lcd_backspace (self);
  i2c_lcd_print_char (self, ' ');
  i2c_lcd_backspace (self);
  }

/*============================================================================
 *  i2c_lcd_new_line
 * ==========================================================================*/
void i2c_lcd_new_line (I2C_LCD *self) 
  {
  if (self->curr_row >= self->height - 1)
    {
    scroll_up (self);
    }
  else
    {
    self->curr_row++;
    }
  i2c_lcd_set_cursor (self, self->curr_row, 0);
  }

/*============================================================================
 *  i2c_lcd_print_char
 * ==========================================================================*/
void i2c_lcd_print_char (I2C_LCD *self, const char c) 
  {
  cancel_scrollback (self);
  switch (c)
    {
    case 8: // BS 
      if (self->destructive_backspace)
        i2c_lcd_del (self); 
      else
        i2c_lcd_backspace (self); 
      break;
    case 10: // LF
      i2c_lcd_line_feed (self); 
      break;
    case 12: // FF (clear screen)
      i2c_lcd_clear (self, TRUE);
      break;
    case 13: // CR
      i2c_lcd_cr (self);
      if (self->implicit_lf)
        i2c_lcd_line_feed (self);
      break;
    case 127: // DEL
      i2c_lcd_del (self); 
      break;
    default:
      send_char (self, c);
      int scrollback_row = self->scrollback_max_lines - self->height 
        + self->curr_row;
      self->scrollback_buffer 
        [scrollback_row * self->width + self->curr_col] = c;
      self->curr_col++;
      if (self->wrap)
        {
        if (self->curr_col >= (int)self->width)
          {
          i2c_lcd_new_line (self);
          }
        }
    }
  }

/*============================================================================
 *  i2c_lcd_clear
 * ==========================================================================*/
void  i2c_lcd_clear (I2C_LCD *self, BOOL clear_scrollback)
  {
  send_command (self, I2C_LCD_CLEAR_DISPLAY); 
  self->curr_row = 0; self->curr_col = 0;

  if (clear_scrollback)
    {
    if (self->scrollback > 0) cancel_scrollback (self);
    reset_scrollback (self);
    }
  }

/*============================================================================
 *  i2c_lcd_wrapping_on
 * ==========================================================================*/
void  i2c_lcd_wrapping_on (I2C_LCD *self)
  {
  self->wrap = TRUE;
  }

/*============================================================================
 *  i2c_lcd_wrapping_off
 * ==========================================================================*/
void  i2c_lcd_wrapping_off (I2C_LCD *self)
  {
  self->wrap = FALSE;
  }

/*============================================================================
 *  i2c_lcd_destructive_backspace_on
 * ==========================================================================*/
void  i2c_lcd_destructive_backspace_on (I2C_LCD *self)
  {
  self->destructive_backspace = TRUE;
  }

/*============================================================================
 *  i2c_lcd_implicit_lf_on
 * ==========================================================================*/
void  i2c_lcd_implicit_lf_on (I2C_LCD *self)
  {
  self->implicit_lf = TRUE;
  }

/*============================================================================
 *  i2c_lcd_implicit_lf_off
 * ==========================================================================*/
void  i2c_lcd_implicit_lf_off (I2C_LCD *self)
  {
  self->implicit_lf = FALSE;
  }

/*============================================================================
 *  i2c_lcd_destructive_backspace_off
 * ==========================================================================*/
void i2c_lcd_destructive_backspace_off (I2C_LCD *self)
  {
  self->destructive_backspace = FALSE;
  }

/*============================================================================
 *  i2c_lcd_scrollback_line_up
 * ==========================================================================*/
void i2c_lcd_scrollback_line_up (I2C_LCD *self)
  {
  int scrollback_start_line = self->scrollback_max_lines - self->height 
    - self->scrollback - 1;

  if (scrollback_start_line > 0)
    {
    self->scrollback++;
    dump_scrollback (self);
    }
  }

/*============================================================================
 *  i2c_lcd_scrollback_line_down
 * ==========================================================================*/
void i2c_lcd_scrollback_line_down (I2C_LCD *self)
  {
  if (self->scrollback > 0)
    {
    self->scrollback--;
    dump_scrollback (self);
    // If we reduce scrollback from 1 to 0, we are effectively at
    //   the working position. So restore the original cursor.
    if (self->scrollback == 0)
      i2c_lcd_set_cursor (self, self->curr_row, self->curr_col);
    }
  }

/*============================================================================
 *  i2c_lcd_destroy
 * ==========================================================================*/
void i2c_lcd_destroy (I2C_LCD* self)
  {
  free (self->scrollback_buffer);
  free (self);
  }



