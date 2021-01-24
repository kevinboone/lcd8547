/*============================================================================
  
  gpiopin.h

  Functions to control a specific GPIO pin. 

  Copyright (c)1990-2020 Kevin Boone. Distributed under the terms of the
  GNU Public Licence, v3.0

  ==========================================================================*/
#pragma once

#include "defs.h"

struct GPIOPin;
typedef struct _GPIOPin GPIOPin;

BEGIN_DECLS

/** Initialize the GPIOPin object with pin number. 
    Note that this method only stores values, 
    and will always succeed. */
GPIOPin  *gpiopin_create (int pin);

/** Clean up the object. This method implicitly calls _uninit(). */
void      gpiopin_destroy (GPIOPin *self);

/** Initialize the object. This opens a file handles for the
    sysfs file for the GPIO pin. Consequently, the method
    can fail. If it does, and *error is not NULL, then it is written with
    and error message that the caller should free. If this method 
    succeeds, _uninit() should be called in due course to clean up. */ 
BOOL      gpiopin_init (GPIOPin *self, char **error);

/** Clean up. In principle, this operation can fail, as it involves sysfs
    operations. But what can we do if this happens? Probably nothing, so no
    errors are reported. */
void      gpiopin_uninit (GPIOPin *self);

/** Set this pin HIGH or LOW. */
void      gpiopin_set (GPIOPin *self, BOOL val);

END_DECLS
