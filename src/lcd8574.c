/*==========================================================================
  
    lcd8574.c

    Implementation of the LCD8574 "class" that is specified in 
    lcd8574.h. This file contains "methods" for initializing the LCD 
    module and writing text to it.

    Datasheet for the HD44780:
    https://www.sparkfun.com/datasheets/LCD/HD44780.pdf

    Datasheet for the PCF8574:
    https://www.ti.com/lit/ds/symlink/pcf8574.pdf

    Copyright (c)2020 Kevin Boone, GPL v3.0

============================================================================*/
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "defs.h" 
#include "gpiopin.h" 
#include "lcd8574.h" 

// Define how the LCD module pins are connected to the PCF8547
//  outputs 0-7 

// Register select -- pin 4 on the LCD module. 0=command, 1=data
#define PIN_RS   0
// Read/write -- pin 5 on the LCD module. 0=write, 1=read. 
// In practice, this pin can usually be set permanently to 0V but
//  if it is connection to an output pin, we must set it low
// Not currently used in the code -- always set to low
#define PIN_RW   1
// Clock (usually called "enable") -- pin 6 on the LCD module. The clock
//  is triggered on the falling edge of this input 
#define PIN_E    2
// Backlight LED anode -- pin 15. The cathode is usually connected to 0V 
//  If the LED is wired permanently on, set this value to -1, so the
//  code won't bother setting it
#define PIN_LED  3 
// Four data pins (pins 11-14). In four-bit mode, 
//  we only use highest four data lines
#define PIN_D4   4
#define PIN_D5   5
#define PIN_D6   6
#define PIN_D7   7
// Pins 7-10 are connected in 4-bit mode


/// ************* LCD commands ************
// Clear display
#define CMD_CLEAR       0x01
// Cursor home 
#define	CMD_HOME	0x02
// Set the entry register
#define	CMD_ENTRY	0x04
// Set the control register
#define	CMD_CTRL	0x08
// Set the screen shift mode register
#define	CMD_CDSHIFT	0x10
// Set the function register
#define	CMD_FUNC	0x20
// Note SET_DDRAM_ADDR is a mask -- the address goes
//  in the bottom 7 bits
#define CMD_SET_DDRAM_ADDR 0x80
// Note SET_CGRAM_ADDR is a mask -- the address goes
//  in the bottom 7 bits
#define CMD_SET_CGRAM_ADDR 0x40

// *** Entry register 
// The "Entry" register (their name, not mine) control what happens
//  to the cursor and layout when characters are printed off the end
//  of a row. In practice, we probably want to take charge of this
//  in software, so these values are not used.
// Shift display if cursor if characters are printed off the end
#define	LCD_ENTRY_SH		0x01
// Increment the cursor position if necessary
#define	LCD_ENTRY_ID		0x02

// *** Function register 

// "Font" -- zero is 5x8 characters, one is 5x10 characters. This 
//    value isn't actually used, because I've never seen a 5x10
//    version of the LCD module (do they even exist?)
#define	LCD_FUNC_F	0x04
// Number of lines -- zero is one line, one is more than one line
#define	LCD_FUNC_N	0x08
// Data Length -- set for 4-bit mode
#define	LCD_FUNC_DL	0x10

#define	LCD_CDSHIFT_RL	0x04

// The number of "addresses" occupied by a single row of text on the
//  display. This will be longer than the number of characters, presumably
//  so that the same controller can be used for different display sizes.
// The value of 64 comes from the datasheet
#define LCD_CHARS_PER_ROW 64

#define I2C_DEV "/dev/i2c-1"

struct _LCD8574
  {
  int i2c_addr;
  int fd; // For the /dev/i2c-1 device
  int rows; int cols;
  BOOL ready;
  };

/*============================================================================
  lcd8574_create
============================================================================*/
LCD8574  *lcd8574_create (int i2c_addr, int rows, int cols)
  {
  LCD8574 *self = malloc (sizeof (LCD8574));
  memset (self, 0, sizeof (LCD8574));
  self->i2c_addr = i2c_addr;
  self->fd = -1;
  self->ready = FALSE;
  self->rows = rows;
  self->cols = cols;
  return self;
  }

/*============================================================================
  lcd8574_destroy
============================================================================*/
void lcd8574_destroy (LCD8574 *self)
  {
  if (self)
    {
    lcd8574_uninit (self);
    free (self);
    }
  }

/*============================================================================

  lcd8574_set_bit_value

  A helper function to set bits in a particular byte 

============================================================================*/
static BYTE lcd8574_set_bit_value (BYTE b, int bit, BOOL val)
  {
  BYTE ret = b;
  if (val)
    ret |= (1 << bit); 
  else
    ret &= ~(1 << bit); 
  return ret;
  }

/*============================================================================

  lcd8574_send_4_bits

  Here's the sequence:
  
  1. Ensure the backlight LED line is on, if a value was specified for it
  2. Set the register select bit, if the caller requires this (this selects
     between command and data registers)
  3. Send the four-bit command and the other (register, backlight) 
     bits on an 8-bit write to the PCF8574, with the E (clock) bit
     high 
  3. Repeat with the clock bit low 

  This is bit fiddly, because we have to write the PCF8574 in 8-bit
  words. What we really want to do is set the RS, LED, and data bits,
  then pulse the E (clock) bit. But we can't, because we can only 
  change the set of 8 PCF8574 outputs in a single operation.

============================================================================*/
static void lcd8574_send_4_bits (LCD8574 *self, BOOL rs, BYTE n)
  {
  BYTE b = (n << 4) & 0xF0;

  if (PIN_LED > 0)
    b = lcd8574_set_bit_value (b, PIN_LED, 1);
  b = lcd8574_set_bit_value (b, PIN_RS, rs);

  // I think we don't need to set E (clock) low every time a command
  //  is sent. It starts off low, then gets pulse high and then low
  //  by this method. So long as we don't accidentally set it high
  //  anywhere else, we don't need to set it low repeatedly. This saves
  //  a couple of milliseconds on each command.
  //b = lcd8574_set_bit_value (b, PIN_E, 0);
  //write (self->fd, &b, 1);
  //usleep (1000);

  b = lcd8574_set_bit_value (b, PIN_E, 1);
  write (self->fd, &b, 1);
  usleep (1000);
  b = lcd8574_set_bit_value (b, PIN_E, 0);
  write (self->fd, &b, 1);
  usleep (1000);
  }

/*============================================================================

  lcd8574_send_byte

  To send a byte in 4-bit mode, we send the high four bits and then the
  low four bits.

============================================================================*/
static void lcd8574_send_byte (LCD8574 *self, BOOL rs, BYTE n)
  {
  lcd8574_send_4_bits (self, rs, (n >> 4) & 0x0F);
  lcd8574_send_4_bits (self, rs, n & 0x0F);
  }

/*============================================================================

  lcd8574_write_char_at

  Use the SET_DDRAM_ADDR command to the LCD module to set the 
  memory address where the char will be written. The send the 
  char as a byte, with the register-select bit high to indicate this
  is data, not a command. 

============================================================================*/
void lcd8574_write_char_at (LCD8574 *self, int row, int col, BYTE c)
  {
  if (row < self->rows && col < self->cols)
    {
    int addr = row * LCD_CHARS_PER_ROW + col; 
    lcd8574_send_byte (self, 0, CMD_SET_DDRAM_ADDR | addr);
    lcd8574_send_byte (self, 1, c);
    }
  }

/*============================================================================

  lcd8574_write_string_at

  Write a whole string, wrapping if necessary. The slightly convoluted
  logic is because the rows of characters are not contiguous in the LCD
  module's memory. Repeated calls to write_char_at would be easier
  to implement, but would send a "set address" command for each character,
  which is wasteful. We only want to set a new address when the text
  wraps to another line.

============================================================================*/
void lcd8574_write_string_at (LCD8574 *self, int row, int col, const BYTE *s,
        BOOL wrap)
  {
  if (row < self->rows && col < self->cols)
    {
    int addr = row * LCD_CHARS_PER_ROW + col; 
    lcd8574_send_byte (self, 0, CMD_SET_DDRAM_ADDR | addr);
    while (*s && row < self->rows && col < self->cols)
      {
      lcd8574_send_byte (self, 1, *s);
      col++;
      if (col >= self->cols && wrap)
	{
	row++;
	col = 0;
	addr = row * LCD_CHARS_PER_ROW + col; 
	lcd8574_send_byte (self, 0, CMD_SET_DDRAM_ADDR | addr);
	}
      s++;
      }
    }
  }

/*============================================================================

  lcd8574_clear

  Just send the clear command.

============================================================================*/
void lcd8574_clear (LCD8574 *self)
  {
  lcd8574_send_byte (self, 0, CMD_CLEAR);
  }

/*============================================================================

  lcd8574_set_cursor
 
  This is a bit hacky, because the HD44780 does have a "move cursor"
  function. The cursor goes right of the last text. So we print an invisible
  null at the cursor position. The null doesn't print anything (and any
  existing character is left alone), but the cursor is placed it
  the specified position. To be frank, I don't know whether all HD44780
  implementations support this. 

============================================================================*/
void lcd8574_set_cursor (LCD8574 *self, int row, int col)
  {
  lcd8574_write_string_at (self, row, col, (BYTE *)"\0", TRUE);
  }

/*============================================================================

  lcd8574_set_mode

  Just send a "control register" command, with the mode bits specified
  by the caller.

============================================================================*/
void lcd8574_set_mode (LCD8574 *self, BYTE mode)
  {
  lcd8574_send_byte (self, 0, CMD_CTRL | mode);
  }

/*============================================================================

  lcd8574_init

  Initialize the display module

============================================================================*/
BOOL lcd8574_init (LCD8574 *self, char **error)
  {
  assert (self != NULL);
  int ret = FALSE;
  // See if we can open the I2C device
  self->fd = open (I2C_DEV, O_WRONLY);
  if (self->fd >= 0)
    {
    // Set the I2C slave address that was supplied when this
    //   object was created
    if (ioctl (self->fd, I2C_SLAVE, self->i2c_addr) >= 0)
      {
      // Set all output PCF8574 lines to zero, because we don't really know
      //  how they will power up
      BYTE c = 0;
      write (self->fd, &c, 1);
      usleep (35000);

      // Now... this is all a bit nasty...
      // We need to set 4-bit mode, but the LCD module powers up in 
      //  eight bit mode. We can't be sure this is the first program
      //  to use the LCD since power-up, so we don't know what 
      //  mode it's in. And we need to issue a command to set 4-bit
      //  mode -- without knowing what mode we're in. So first we have
      //  to enable 8-bit mode and then, knowing we're in 8-bit mode,
      //  we must set 4-bit mode. Setting 8-bit mode without knowing the
      //  current mode can be accomplished by sending the mode-setting
      //  command as three identical 4-bit commands. If we start in 
      //  8-bit mode, some of these commands are gibberish 8-bit 
      //  commands with four of their bits set wrongly. But there's still
      //  enough coherence for the module to get the message with thi
      //  command sequence. This method of setting the mode is widely
      //  used, even though it isn't documented, and it seems to work OK. 

      BYTE func = CMD_FUNC | LCD_FUNC_DL; // Set 8-bit mode
      lcd8574_send_4_bits (self, 0, func >> 4); usleep (35000);
      lcd8574_send_4_bits (self, 0, func >> 4); usleep (35000);
      lcd8574_send_4_bits (self, 0, func >> 4); usleep (35000);
      func = CMD_FUNC | 0; // Set 4-bit mode
      lcd8574_send_4_bits (self, 0, func >> 4); usleep (35000);

      // Set more than one row (the LCD only has two line modes, 
      //  "one" or "more that one")
      func = CMD_FUNC | LCD_FUNC_N ;
      // NB -- send_byte sends two 4-bit commands in a row
      lcd8574_send_byte (self, 0, func); 

      // Clear display 
      lcd8574_clear (self);
      lcd8574_set_mode (self, LCD_MODE_DISPLAY_ON);

      // We might want to set the cursor and shift modes -- but, honestly,
      //   it's more likely that the user of this class will take care of 
      //   these things. 
      //lcd8574_send_byte (self, 0, CMD_ENTRY | LCD_ENTRY_ID);
      //lcd8574_send_byte (self, 0, CMD_CDSHIFT | LCD_CDSHIFT_RL);

      ret = TRUE;
      self->ready = TRUE;
      }
    else
      {
      asprintf (error, "Can't intialize I2C device: %s", strerror (errno));
      }
    }
  else
    {
    asprintf (error, "Can't open I2C device: %s", strerror (errno));
    }
  return ret;
  }

/*============================================================================
  lcd8574_uninit
============================================================================*/
void lcd8574_uninit (LCD8574 *self)
  {
  assert (self != NULL);
  if (self->fd >= 0) close (self->fd);
  self->ready = FALSE;
  }

