#include "i2c_reg.h"

#define I2C_WAIT_LIMIT  1000000UL
#define I2C_ERROR_MASK  (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR)

static I2C_RegStatus I2C1_WaitSr1Set(uint32_t flag)
{
  uint32_t timeout = I2C_WAIT_LIMIT;

  while ((I2C1->SR1 & flag) == 0U)
  {
    uint32_t errors = I2C1->SR1 & I2C_ERROR_MASK;

    if ((errors & I2C_SR1_AF) != 0U)
    {
      return I2C_REG_NACK;
    }
    if (errors != 0U)
    {
      return I2C_REG_BUS_ERROR;
    }
    if (--timeout == 0U)
    {
      return I2C_REG_TIMEOUT;
    }
  }

  return I2C_REG_OK;
}

static I2C_RegStatus I2C1_WaitBusIdle(void)
{
  uint32_t timeout = I2C_WAIT_LIMIT;

  while ((I2C1->SR2 & I2C_SR2_BUSY) != 0U)
  {
    if (--timeout == 0U)
    {
      return I2C_REG_TIMEOUT;
    }
  }

  return I2C_REG_OK;
}

static I2C_RegStatus I2C1_Abort(I2C_RegStatus status)
{
  I2C1->CR1 |= I2C_CR1_STOP;
  I2C1->SR1 &= ~I2C_ERROR_MASK;
  return status;
}

I2C_RegStatus I2C1_MasterTransmit(uint8_t address_7bit,
                                 const uint8_t *data,
                                 size_t length)
{
  I2C_RegStatus status;
  volatile uint32_t clear_addr;
  size_t index;

  if ((data == NULL) || (length == 0U) || (address_7bit > 0x7FU))
  {
    return I2C_REG_BUS_ERROR;
  }

  I2C1->SR1 &= ~I2C_ERROR_MASK;

  status = I2C1_WaitBusIdle();
  if (status != I2C_REG_OK)
  {
    return I2C1_Abort(status);
  }

  I2C1->CR1 |= I2C_CR1_START;
  status = I2C1_WaitSr1Set(I2C_SR1_SB);
  if (status != I2C_REG_OK)
  {
    return I2C1_Abort(status);
  }

  I2C1->DR = ((uint32_t)address_7bit << 1U);
  status = I2C1_WaitSr1Set(I2C_SR1_ADDR);
  if (status != I2C_REG_OK)
  {
    return I2C1_Abort(status);
  }

  /* Reading SR1 followed by SR2 clears ADDR. */
  clear_addr = I2C1->SR1;
  clear_addr = I2C1->SR2;
  (void)clear_addr;

  for (index = 0U; index < length; ++index)
  {
    status = I2C1_WaitSr1Set(I2C_SR1_TXE);
    if (status != I2C_REG_OK)
    {
      return I2C1_Abort(status);
    }
    I2C1->DR = data[index];
  }

  status = I2C1_WaitSr1Set(I2C_SR1_BTF);
  if (status != I2C_REG_OK)
  {
    return I2C1_Abort(status);
  }

  I2C1->CR1 |= I2C_CR1_STOP;
  return I2C_REG_OK;
}
