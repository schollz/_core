/*===========================================================================
 * usb_kbd/usb_kbd.h
 * Copyright (c)2022 Kevin Boone, GPL v3.0
 * ========================================================================*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Initalize the USB host support. */
extern void usb_kbd_init (void);

/** Read the USB input queue and dispatch callback functions. */
extern void usb_kbd_scan (void);

#ifdef __cplusplus
}
#endif


