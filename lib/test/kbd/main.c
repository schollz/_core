/*===========================================================================
 * main.c
 * Copyright (c)2022 Kevin Boone, GPL v3.0
 * ========================================================================*/

#include <hardware/i2c.h>
#include <kbd/kbd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usb_kbd/usb_kbd.h>

#include "../../ssd1306.h"
#include "bsp/board.h"
#include "config.h"
#define I2C_SDA_PIN 20
#define I2C_SCL_PIN 21
ssd1306_t disp;

/*===========================================================================
 * blink_led_task
 * Called in the main scanning loop. We flash the LED just to indicate that
 * the program hasn't crashed.
 * ========================================================================*/
void blink_led_task(void) {
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;
  static bool led_state = false;
  if (board_millis() - start_ms < interval_ms) {
    return;
  }
  start_ms += interval_ms;
  board_led_write(led_state);
  led_state = !led_state;
  ssd1306_clear(&disp);

  sleep_ms(10);
  char s[10];
  sprintf(s, "%d", led_state);
  ssd1306_draw_string(&disp, 8, 24, 2, s);
  ssd1306_show(&disp);
  sleep_ms(10);
}

/*===========================================================================
 * kbd_raw_key_down is called by the USB HID code whenever a
 * key is pressed. The value 'code' does not take account of which
 * modifiers are pressed -- call kbd_to_ascii() to deal wity that.
 * ========================================================================*/
void kbd_raw_key_down(int code, int flags) {
  char s[10];
  sprintf(s, "%d %02X ", code, flags);
  ssd1306_clear(&disp);
  sleep_ms(500);
  ssd1306_draw_string(&disp, 8, 24, 2, s);
  ssd1306_show(&disp);
  sleep_ms(500);
  // switch (code) {
  //   // Handle scrollback using up/down keys. All other keys, pass
  //   //   straight through to the display.
  //   case KBD_KEY_UP:
  //     // i2c_lcd_scrollback_line_up(i2c_lcd);
  //     break;
  //   case KBD_KEY_DOWN:
  //     // i2c_lcd_scrollback_line_down(i2c_lcd);
  //     break;
  //   // TODO scrollback page up/down
  //   default:
  //     char c = kbd_to_ascii(code, flags);
  //     ssd1306_clear(&disp);
  //     ssd1306_draw_string(&disp, 8, 24, 2, c);
  //     ssd1306_show(&disp);
  // }
}

/*===========================================================================
 * start here
 * ========================================================================*/
int main(void) {
  stdio_init_all();
  sleep_ms(1000);

  // This example will use I2C0 on the default SDA and SCL pins (GP4, GP5 on a
  // Pico)
  i2c_init(i2c_default, 40 * 1000);
  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);

  disp.external_vcc = false;
  ssd1306_init(&disp, 128, 64, 0x3C, i2c_default);
  ssd1306_clear(&disp);
  ssd1306_draw_string(&disp, 8, 24, 2, "hello world");
  ssd1306_show(&disp);
  sleep_ms(1000);

  usb_kbd_init();

  // Loop, dispatching USB events to the handler and blinking the LED
  while (1) {
    usb_kbd_scan();
    blink_led_task();
  }
}
