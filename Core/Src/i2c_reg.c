#include "i2c_reg.h"

#define I2C_TRANSFER_TIMEOUT_MS  100U
#define I2C_ERROR_MASK  (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR)
#define I2C_INTERRUPT_MASK \
  (I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN)
#define I2C1_TX_DMA_STREAM       DMA1_Stream6
#define I2C1_TX_DMA_CHANNEL      (1U << DMA_SxCR_CHSEL_Pos)
#define I2C1_TX_DMA_ERROR_FLAGS  (DMA_HISR_TEIF6 | DMA_HISR_DMEIF6)
#define I2C1_TX_DMA_ALL_FLAGS \
  (DMA_HIFCR_CTCIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTEIF6 | \
   DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6)

typedef struct
{
  const uint8_t *data;
  size_t length;
  volatile size_t index;
  uint8_t address_byte;
  volatile I2C_RegStatus status;
  volatile bool busy;
  volatile bool use_dma;
} I2C_TransferState;

static I2C_TransferState transfer;

static void I2C1_DisableTransferInterrupts(void)
{
  I2C1->CR2 &= ~I2C_INTERRUPT_MASK;
}

static void I2C1_StopTxDMA(void)
{
  I2C1->CR2 &= ~I2C_CR2_DMAEN;
  I2C1_TX_DMA_STREAM->CR &= ~DMA_SxCR_EN;
  while ((I2C1_TX_DMA_STREAM->CR & DMA_SxCR_EN) != 0U)
  {
  }
  DMA1->HIFCR = I2C1_TX_DMA_ALL_FLAGS;
}

static void I2C1_FinishTransfer(I2C_RegStatus status)
{
  I2C1_DisableTransferInterrupts();
  I2C1_StopTxDMA();
  I2C1->CR1 |= I2C_CR1_STOP;
  transfer.status = status;
  __DMB();
  transfer.busy = false;
}

static void I2C1_AbortTransfer(I2C_RegStatus status)
{
  I2C1_DisableTransferInterrupts();
  I2C1_StopTxDMA();
  I2C1->CR1 |= I2C_CR1_STOP;
  I2C1->SR1 &= ~I2C_ERROR_MASK;
  transfer.status = status;
  __DMB();
  transfer.busy = false;
}

I2C_RegStatus I2C1_MasterTransmitIT(uint8_t address_7bit,
                                   const uint8_t *data,
                                   size_t length)
{
  if ((data == NULL) || (length == 0U) || (address_7bit > 0x7FU))
  {
    return I2C_REG_BUS_ERROR;
  }

  if (transfer.busy || ((I2C1->SR2 & I2C_SR2_BUSY) != 0U))
  {
    return I2C_REG_BUSY;
  }

  I2C1->SR1 &= ~I2C_ERROR_MASK;
  transfer.data = data;
  transfer.length = length;
  transfer.index = 0U;
  transfer.address_byte = (uint8_t)(address_7bit << 1U);
  transfer.status = I2C_REG_BUSY;
  transfer.busy = true;
  transfer.use_dma = false;
  __DMB();

  /*
   * Event IRQ handles SB, ADDR and BTF.
   * Buffer IRQ additionally generates TXE interrupts.
   * Error IRQ handles NACK, arbitration loss and bus errors.
   */
  I2C1->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;
  I2C1->CR1 |= I2C_CR1_START;

  return I2C_REG_OK;
}

I2C_RegStatus I2C1_MasterTransmitDMA(uint8_t address_7bit,
                                    const uint8_t *data,
                                    size_t length)
{
  if ((data == NULL) || (length == 0U) || (length > 0xFFFFU) ||
      (address_7bit > 0x7FU))
  {
    return I2C_REG_BUS_ERROR;
  }

  if (transfer.busy || ((I2C1->SR2 & I2C_SR2_BUSY) != 0U))
  {
    return I2C_REG_BUSY;
  }

  I2C1_StopTxDMA();
  I2C1->SR1 &= ~I2C_ERROR_MASK;

  I2C1_TX_DMA_STREAM->PAR = (uint32_t)&I2C1->DR;
  I2C1_TX_DMA_STREAM->M0AR = (uint32_t)data;
  I2C1_TX_DMA_STREAM->NDTR = (uint32_t)length;
  I2C1_TX_DMA_STREAM->CR =
      I2C1_TX_DMA_CHANNEL |
      DMA_SxCR_MINC |
      DMA_SxCR_DIR_0 |
      DMA_SxCR_PL_1 |
      DMA_SxCR_TCIE |
      DMA_SxCR_TEIE |
      DMA_SxCR_DMEIE;
  /*
   * Byte 단위 I2C 전송은 DMA direct mode를 사용한다. FIFO를 사용하지
   * 않으므로 FEIE를 켜면 안 된다.
   */
  I2C1_TX_DMA_STREAM->FCR = 0U;

  transfer.data = data;
  transfer.length = length;
  transfer.index = 0U;
  transfer.address_byte = (uint8_t)(address_7bit << 1U);
  transfer.status = I2C_REG_BUSY;
  transfer.busy = true;
  transfer.use_dma = true;
  __DMB();

  /*
   * START와 주소 전송은 I2C event IRQ가 처리한다. ADDR가 확인되면
   * DMA1 Stream 6이 메모리의 데이터를 I2C1->DR로 전송한다.
   */
  I2C1->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;
  I2C1->CR1 |= I2C_CR1_START;

  return I2C_REG_OK;
}

I2C_RegStatus I2C1_MasterTransmit(uint8_t address_7bit,
                                 const uint8_t *data,
                                 size_t length)
{
  I2C_RegStatus status;
  uint32_t start_tick = HAL_GetTick();

  /*
   * STOP may need a few peripheral clocks before BUSY clears. Wait without
   * touching the data-path flags; SysTick wakes WFI once per millisecond.
   */
  while ((I2C1->SR2 & I2C_SR2_BUSY) != 0U)
  {
    if ((HAL_GetTick() - start_tick) >= I2C_TRANSFER_TIMEOUT_MS)
    {
      return I2C_REG_TIMEOUT;
    }
    __WFI();
  }

  status = I2C1_MasterTransmitDMA(address_7bit, data, length);
  if (status != I2C_REG_OK)
  {
    return status;
  }

  start_tick = HAL_GetTick();
  while (transfer.busy)
  {
    if ((HAL_GetTick() - start_tick) >= I2C_TRANSFER_TIMEOUT_MS)
    {
      I2C1_AbortTransfer(I2C_REG_TIMEOUT);
      break;
    }
    __WFI();
  }

  return transfer.status;
}

bool I2C1_IsBusy(void)
{
  return transfer.busy;
}

void I2C1_EventIRQHandler(void)
{
  uint32_t sr1 = I2C1->SR1;

  if (!transfer.busy)
  {
    I2C1_DisableTransferInterrupts();
    return;
  }

  if ((sr1 & I2C_SR1_SB) != 0U)
  {
    I2C1->DR = transfer.address_byte;
    return;
  }

  if ((sr1 & I2C_SR1_ADDR) != 0U)
  {
    volatile uint32_t clear_addr;

    clear_addr = I2C1->SR1;
    clear_addr = I2C1->SR2;
    (void)clear_addr;

    if (transfer.use_dma)
    {
      I2C1_TX_DMA_STREAM->CR |= DMA_SxCR_EN;
      I2C1->CR2 |= I2C_CR2_DMAEN;
    }
    else
    {
      I2C1->CR2 |= I2C_CR2_ITBUFEN;
    }
    return;
  }

  if (((sr1 & I2C_SR1_BTF) != 0U) &&
      (transfer.index >= transfer.length))
  {
    I2C1_FinishTransfer(I2C_REG_OK);
    return;
  }

  if (((sr1 & I2C_SR1_TXE) != 0U) &&
      (transfer.index < transfer.length))
  {
    I2C1->DR = transfer.data[transfer.index];
    ++transfer.index;

    /*
     * After the final byte, stop TXE interrupts and let BTF generate the
     * final event interrupt when both DR and the shift register are empty.
     */
    if (transfer.index >= transfer.length)
    {
      I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
    }
  }
}

void I2C1_ErrorIRQHandler(void)
{
  uint32_t errors = I2C1->SR1 & I2C_ERROR_MASK;
  I2C_RegStatus status = I2C_REG_BUS_ERROR;

  if ((errors & I2C_SR1_AF) != 0U)
  {
    status = I2C_REG_NACK;
  }

  I2C1->SR1 &= ~errors;
  I2C1_AbortTransfer(status);
}

void I2C1_TX_DMAIRQHandler(void)
{
  uint32_t status = DMA1->HISR;

  if ((status & I2C1_TX_DMA_ERROR_FLAGS) != 0U)
  {
    I2C1_AbortTransfer(I2C_REG_BUS_ERROR);
    return;
  }

  if ((status & DMA_HISR_TCIF6) != 0U)
  {
    I2C1_StopTxDMA();
    transfer.index = transfer.length;

    /*
     * DMA 완료는 마지막 바이트가 DR에 기록되었다는 뜻이다.
     * I2C의 BTF 이벤트가 마지막 바이트의 실제 전송 완료를 알리므로,
     * STOP은 I2C event IRQ에서 생성한다.
     */
  }
}
