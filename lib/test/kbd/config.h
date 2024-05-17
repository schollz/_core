/*===========================================================================
 * config.h
 *
 * General configuration values for the hardware
 *
 * Copyright (c)2022 Kevin Boone, GPL v3.0
 * ========================================================================*/

#pragma once

// Size of the scrollback buffer, in screen pages. That is, with a 
//   four-line display, a page is four lines. There's no problem with
//   increasing this, except memory.
#define SCROLLBACK_PAGES 10

// The I2C address of the I2C LCD device. Common values are 0x27 and 0x3F. 
// Some devices can have their I2C addresses set using jumpers.
#define I2C_LCD_ADDRESS 0x27

// I2C baud rate. 100k seems a safe value. With short, tidy connections,
//   this could usefully be set higher.
#define I2C_BAUD 100000

// The size of the LCD. Common sizes are 16x2, 20x4 and 40x2
#define LCD_WIDTH  16
#define LCD_HEIGHT 2


