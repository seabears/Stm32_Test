#include "main.h"
#include "usbd_dfu_if.h"
#include <string.h>

volatile uint8_t g_dfu_activity_seen = 0U;

static uint16_t DFU_Flash_Init(void);
static uint16_t DFU_Flash_DeInit(void);
static uint16_t DFU_Flash_Erase(uint32_t Add);
static uint16_t DFU_Flash_Write(uint8_t *src, uint8_t *dest, uint32_t Len);
static uint8_t *DFU_Flash_Read(uint8_t *src, uint8_t *dest, uint32_t Len);
static uint16_t DFU_Flash_GetStatus(uint32_t Add, uint8_t cmd, uint8_t *buff);
static uint8_t DFU_AddressAllowed(uint32_t address, uint32_t length);
static void Flash_WaitNotBusy(void);
static void Flash_ClearFlags(void);
static void Flash_Unlock(void);
static void Flash_Lock(void);
static uint8_t Flash_ErasePage(uint32_t page_address);
static uint8_t Flash_ProgramHalfword(uint32_t address, uint16_t data);

USBD_DFU_MediaTypeDef USBD_DFU_Flash_fops =
{
  (uint8_t *)"@Internal Flash /0x08004000/112*001Kg",
  DFU_Flash_Init,
  DFU_Flash_DeInit,
  DFU_Flash_Erase,
  DFU_Flash_Write,
  DFU_Flash_Read,
  DFU_Flash_GetStatus
};

static uint16_t DFU_Flash_Init(void)
{
  Flash_Unlock();
  return USBD_OK;
}

static uint16_t DFU_Flash_DeInit(void)
{
  Flash_Lock();
  return USBD_OK;
}

static uint16_t DFU_Flash_Erase(uint32_t Add)
{
  uint32_t page_address = Add - (Add % FLASH_PAGE_SIZE);

  g_dfu_activity_seen = 1U;

  if (DFU_AddressAllowed(page_address, FLASH_PAGE_SIZE) == 0U)
  {
    return USBD_FAIL;
  }

  return (Flash_ErasePage(page_address) != 0U) ? USBD_OK : USBD_FAIL;
}

static uint16_t DFU_Flash_Write(uint8_t *src, uint8_t *dest, uint32_t Len)
{
  uint32_t address = (uint32_t)dest;
  uint32_t i = 0U;

  g_dfu_activity_seen = 1U;

  if (DFU_AddressAllowed(address, Len) == 0U)
  {
    return USBD_FAIL;
  }

  while (i < Len)
  {
    uint16_t halfword = src[i];
    if ((i + 1U) < Len)
    {
      halfword |= ((uint16_t)src[i + 1U] << 8);
    }
    else
    {
      halfword |= 0xFF00U;
    }

    if (Flash_ProgramHalfword(address + i, halfword) == 0U)
    {
      return USBD_FAIL;
    }
    i += 2U;
  }

  return USBD_OK;
}

static uint8_t *DFU_Flash_Read(uint8_t *src, uint8_t *dest, uint32_t Len)
{
  if (DFU_AddressAllowed((uint32_t)src, Len) == 0U)
  {
    memset(dest, 0xFF, Len);
    return dest;
  }

  memcpy(dest, src, Len);
  return dest;
}

static uint16_t DFU_Flash_GetStatus(uint32_t Add, uint8_t cmd, uint8_t *buff)
{
  (void)Add;

  if (cmd == DFU_MEDIA_ERASE)
  {
    buff[1] = 2U;
    buff[2] = 0U;
    buff[3] = 0U;
  }
  else
  {
    buff[1] = 1U;
    buff[2] = 0U;
    buff[3] = 0U;
  }
  return USBD_OK;
}

static uint8_t DFU_AddressAllowed(uint32_t address, uint32_t length)
{
  if (address < APP_ADDRESS)
  {
    return 0U;
  }
  if (address >= FLASH_END_ADDRESS)
  {
    return 0U;
  }
  if (length > (FLASH_END_ADDRESS - address))
  {
    return 0U;
  }
  return 1U;
}

static void Flash_WaitNotBusy(void)
{
  while ((FLASH->SR & FLASH_SR_BSY) != 0U)
  {
  }
}

static void Flash_ClearFlags(void)
{
  FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
}

static void Flash_Unlock(void)
{
  if ((FLASH->CR & FLASH_CR_LOCK) != 0U)
  {
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
  }
}

static void Flash_Lock(void)
{
  FLASH->CR |= FLASH_CR_LOCK;
}

static uint8_t Flash_ErasePage(uint32_t page_address)
{
  Flash_WaitNotBusy();
  Flash_ClearFlags();

  FLASH->CR |= FLASH_CR_PER;
  FLASH->AR = page_address;
  FLASH->CR |= FLASH_CR_STRT;

  Flash_WaitNotBusy();
  FLASH->CR &= ~FLASH_CR_PER;

  return ((FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) == 0U) ? 1U : 0U;
}

static uint8_t Flash_ProgramHalfword(uint32_t address, uint16_t data)
{
  Flash_WaitNotBusy();
  Flash_ClearFlags();

  FLASH->CR |= FLASH_CR_PG;
  *(__IO uint16_t *)address = data;

  Flash_WaitNotBusy();
  FLASH->CR &= ~FLASH_CR_PG;

  if ((FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0U)
  {
    return 0U;
  }
  return (*(__IO uint16_t *)address == data) ? 1U : 0U;
}