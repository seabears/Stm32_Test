#ifndef I2C_REG_H
#define I2C_REG_H

#include "main.h"

#include <stddef.h>
#include <stdint.h>

typedef enum
{
  I2C_REG_OK = 0,
  I2C_REG_TIMEOUT,
  I2C_REG_NACK,
  I2C_REG_BUS_ERROR
} I2C_RegStatus;

I2C_RegStatus I2C1_MasterTransmit(uint8_t address_7bit,
                                 const uint8_t *data,
                                 size_t length);

#endif /* I2C_REG_H */
