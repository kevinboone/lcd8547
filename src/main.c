/*============================================================================
  
    main.c

    A test driver for the LCD8574 "class". It just displays the current
    time and date on the LCD.

    Copyright (c)2020 Kevin Boone, GPL v3.0

============================================================================*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "defs.h" 
#include "lcd8574.h" 

#define I2C_ADDR 0x27
#define ROWS 2
#define COLS 16


/*============================================================================

  main

============================================================================*/
int main (int argc, char **argv)
  {
  argc = argc; argv = argv; // Suppress warnings

  // Set up the LCD8574 instance with the three GPIO pin numbers.
  LCD8574 *hc = lcd8574_create (I2C_ADDR, ROWS, COLS);
  char *error = NULL;
  if (lcd8574_init (hc, &error))
    {
    // Note that because the text we're writing is a fixed length,
    //  there's no need to clear the display before refreshing it --
    //  we just write the old text on top of the new. This causes
    //  less flicker than a full refresh.
    lcd8574_clear (hc);
    while (TRUE)
      {
      // Write the whole output. In a real appliation, we'd only write
      //  the character cells that had changed between updates, to
      //  save time and reduce flicker
      char s[50];
      time_t t = time (NULL);
      struct tm *tm = localtime (&t);
      sprintf (s, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
      lcd8574_write_string_at (hc, 0, 0, (BYTE *)s, FALSE);
      sprintf (s, "%04d/%02d/%02d", tm->tm_year + 1900, tm->tm_mon + 1, 
        tm->tm_mday);
      lcd8574_write_string_at (hc, 1, 0, (BYTE *)s, FALSE);
      usleep (1000000);
      }

    lcd8574_uninit (hc);
    lcd8574_destroy (hc);
    }
  else
    {
    fprintf (stderr, "%s: %s\n", argv[0], error);
    free (error);
    }
  }


