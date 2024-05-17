/*===========================================================================
 * usb_kbd/kbd.c
 *
 * General keyboard handling routines 
 *
 * Copyright (c)2022 Kevin Boone, GPL v3.0
 * ========================================================================*/

#include <kbd/kbd.h>

/*===========================================================================
 * kbd_to_ascii
 * ========================================================================*/
char kbd_to_ascii (int code, int flags)
  {
  // TODO -- I'm not sure how we handle 8-bit key codes, if there 
  //   are any. I'm sure some keyboards must generate them (or, at least,
  //   there scancode-to-keycode mappings do. At present, we only
  //   support US keyboards. Virtual keys like 'up' and 'F1' also
  //   generate codes > 127, and these also cannot meaningfully be
  //   converted.
  if (code > 127) return 0;
  
  // We have more work to do here. What about shift-ctrl, shift-alt, etc?
  if (flags & KBD_FLAG_CONTROL)
     return (code & ~0x60);

  return code; 
  }



