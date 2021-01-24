/*==========================================================================
  
    gpiopin.c

    A "class" for setting values of specific GPIO pins.

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
#include "defs.h" 
#include "gpiopin.h" 

struct _GPIOPin
  {
  int pin; 
  int value_fd;
  };

/*============================================================================
  gpiopin_create
============================================================================*/
GPIOPin *gpiopin_create (int pin)
  {
  GPIOPin *self = malloc (sizeof (GPIOPin));
  memset (self, 0, sizeof (GPIOPin));
  self->pin = pin;
  self->value_fd = -1;
  return self;
  }

/*============================================================================
  gpiopin_write_to_file
============================================================================*/
static BOOL gpiopin_write_to_file (const char *filename, 
    const char *text, char **error)
  {
  assert (filename != NULL);
  assert (text != NULL);
  BOOL ret = FALSE;
  FILE *f = fopen (filename, "w");
  if (f)
    {
    fprintf (f, text);
    fclose (f);
    ret = TRUE;
    }
  else
    {
    if (error)
      asprintf (error, "Can't open %s for writing: %s", filename, 
        strerror (errno));
    ret = FALSE;
    }
  return ret;
  }


/*============================================================================
  gpiopin_destroy
============================================================================*/
void gpiopin_destroy (GPIOPin *self)
  {
  if (self)
    {
    gpiopin_uninit (self);
    free (self);
    }
  }

/*============================================================================
  gpiopin_init
============================================================================*/
BOOL gpiopin_init (GPIOPin *self, char **error)
  {
  assert (self != NULL);
  char s[50];
  snprintf (s, sizeof(s), "%d", self->pin);
  BOOL ret = gpiopin_write_to_file ("/sys/class/gpio/export", s, error);
  if (ret)
    {
    char s[50];
    snprintf (s, sizeof(s), "/sys/class/gpio/gpio%d/direction", self->pin);
    gpiopin_write_to_file (s, "out", NULL); // TODO: add input capability :)
    snprintf (s, sizeof(s), "/sys/class/gpio/gpio%d/value", self->pin);
    self->value_fd = open (s, O_WRONLY);
    if (self->value_fd >= 0) 
      {
      ret = TRUE;
      }
    else
      {
      if (error)
        asprintf (error, "Can't open %s for writing: %s", s, strerror (errno));
      ret = FALSE;
      }
    }
  return ret;
  }

/*============================================================================
  gpiopin_uninit
============================================================================*/
void gpiopin_uninit (GPIOPin *self)
  {
  assert (self != NULL);
  if (self->value_fd >= 0)
    close (self->value_fd);
  self->value_fd = -1;
  char s[50];
  snprintf (s, sizeof(s), "%d", self->pin);
  gpiopin_write_to_file ("/sys/class/gpio/unexport", s, NULL);
  }

/*============================================================================
  gpiopin_set
============================================================================*/
void gpiopin_set (GPIOPin *self, BOOL val)
  {
  assert (self != NULL);
  assert (self->value_fd >= 0);
  char c = val ? '1' : '0';
  write (self->value_fd, &c, 1);
  }


