/*===========================================================================
 * kbd/kbd.h
 * Copyright (c)2022 Kevin Boone, GPL v3.0
 * ========================================================================*/

#pragma once

#define KBD_FLAG_SHIFT  0x01
#define KBD_FLAG_CONTROL  0x02
#define KBD_FLAG_ALT  0x04

#define KBD_KEY_BS 8
#define KBD_KEY_ENTER 13
#define KBD_KEY_DOWN 1000
#define KBD_KEY_UP 1001
#define KBD_KEY_PGDN 1002
#define KBD_KEY_PGUP 1003
#define KBD_KEY_RIGHT 1004
#define KBD_KEY_LEFT 1005
#define KBD_KEY_HOME 1006
#define KBD_KEY_END 1007

#ifdef __cplusplus
extern "C" {
#endif

/* raw_key_down should be called whenever a key is pressed, anywhere.
 * The code should distinguish upper and lower case letters, and
 * symbols that appear on the same key, but no other processing.
 * For example, ctrl-A is 'A' with a flag to indicate that ctrl
 * is pressed. */
extern void kbd_raw_key_down (int code, int flags);

/* Convert the code and flags from raw_key_down to an ASCII value, if
   possible. If no conversion is possible (e.g., it's an arrow key) 
   then return zero. */
extern char kbd_to_ascii (int code, int flags);

#ifdef __cplusplus
}
#endif



