/*============================================================================
  
  lcd8574.h

  Functions to control an HD44780 LCD module via an PCF8574 I2C-to-parallel
  controller. The LCD controller is operated in 4-bit mode, so all the
  necessary pins can be connected to one PCF8574 device, which has only
  eight digitial outputs. 

  There are many ways to connect the PCF8574 to the HD8840. Please see
  the definitions at the top of lcd8574.c, to see typical connections 
  (or edit this file if your connections are different).

  This "class" provides the most basic functions available for the
  HD88470 LCD module -- initialization, writing text at specific
  places, and controlling the cursor. It's difficult to provide more
  specialized functions, because these would depend entirely on the
  application.

  Note that there are no methods in this class to control the LCD backlight,
  because the module is essentially useless with it switched off. If a
  pin is wired to the backlight, the code will turn it permanently on. 
  In addition, although both the PCF8574 and the HD44780 have data-read
  operations, this code makes on use of them. In the modules R/W pin
  in connected, it is set permanently low, for write mode. 

  Copyright (c)1990-2020 Kevin Boone. Distributed under the terms of the
  GNU Public Licence, v3.0

  ==========================================================================*/
#pragma once

#include "defs.h"


// Flags for use with lcd8574_set_mode(). 
#define	LCD_MODE_CURSOR_BLINK   0x01
#define	LCD_MODE_CURSOR_ON      0x02
#define	LCD_MODE_DISPLAY_ON	0x04


struct LCD8574;
typedef struct _LCD8574 LCD8574;

BEGIN_DECLS

/** Initialize the LCD8574 object with the numbers of the three GPIO
    pins that will be used. Note that this method only stores values, 
    and will always succeed. The caller should specify the size of
    the LCD, because this cannot be worked out by interrogating
    the device. The sizes are only used to prevent writing off the
    ends or bottom of the LCD.  */
LCD8574  *lcd8574_create (int i2c_addr, int rows, int cols);

/** Clean up this object. This method implicitly calls _uninit(). */
void      lcd8574_destroy (LCD8574 *self);

/** Initialize this object. This opens a file handle for the
    I2C device and keeps it open until _unint() is called. This method 
    can fail. If it does, and *error is not NULL, then it is written with
    and error message that the caller should free. If this method 
    succeeds, _uninit() should be called in due course to clean up. */ 
BOOL      lcd8574_init (LCD8574 *self, char **error);

/** Clean up. In principle, this operation can fail, as it involves device 
    operations. But what can we do if this happens? Probably nothing, so no
    errors are reported. */
void      lcd8574_uninit (LCD8574 *self);

/** Write a character at the specified position. Note that the LCD device 
    has, by default, a character set that is a kind of modified ASCII. 
    The method will do nothing if the specific row and column are out of
    range. */
void      lcd8574_write_char_at (LCD8574 *self, int row, int col, BYTE c);

/** Write a string of ASCII(-ish) characters, starting at the specified
    position. If wrap is set, output will continue on the next line
    if it reaches the end of the first. However, there is no scrolling if
    if reach the end of the last line. */
void      lcd8574_write_string_at (LCD8574 *self, int row, int col, 
            const BYTE *s, BOOL wrap);

void      lcd8574_clear (LCD8574 *self);

/** Sets the display mode control register. This allows the display to
    be turned on and off, and the cursor mode to be set. These functions
    don't naturally go together -- they just happen to be sent to the
    LCD device in the same command byte. Note that the modes are not
    cummulative -- you have to set them all in one operation, which isn't
    hugely convenient. */ 
void      lcd8574_set_mode (LCD8574 *self, BYTE mode);

/** Set the cursor position. The cursor must have been set visible for
    this method to show any effect. Note that the HD44780 LCD module does
    not have a specific method to set the cursor position -- it just follows
    the text. The method I use is a hack. */
void      lcd8574_set_cursor (LCD8574 *self, int row, int col);

END_DECLS

