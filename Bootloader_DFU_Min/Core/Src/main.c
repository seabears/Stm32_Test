#include "stm32f1xx.h"

/*
 * STM32F103 최소 USB DFU 부트로더.
 *
 * 플래시 배치:
 *   0x08000000 ~ 0x08001FFF : 이 부트로더, 목표 크기 8 KiB
 *   0x08002000 ~ 끝         : USB DFU로 기록되는 사용자 앱
 *
 * 전체 동작:
 *   1. 클럭, SysTick, USB 장치 주변장치를 직접 초기화한다.
 *   2. 리셋 직후 짧은 시간 동안 USB DFU 장치 1EAF:0003으로 나타난다.
 *   3. dfu-util 업로드가 들어오면 앱 영역만 지우고 기록한다.
 *   4. 업로드가 없고 기존 앱이 유효하면 앱 벡터 테이블로 점프한다.
 */

/* 사용자 앱 시작 주소. 앱 링커 스크립트도 반드시 이 주소에 맞아야 한다. */
#define APP_ADDRESS        0x08002000U
/* STM32F103C8 128 KiB 플래시의 끝 주소. 기록 범위 검사에 사용한다. */
#define FLASH_END_ADDRESS  0x08020000U
/* 앱 벡터 테이블의 초기 SP가 SRAM 안에 있는지 검사하기 위한 범위. */
#define SRAM_START         0x20000000U
#define SRAM_END           0x20005000U
/* DFU 활동이 없으면 이 시간 뒤 앱으로 점프한다. */
#define BOOT_TIMEOUT_MS    1000U
/* 디버깅용. 1로 바꾸면 앱으로 점프하지 않고 DFU 모드에 계속 머문다. */
#define DEBUG_STAY_IN_DFU  0U

/* Maple DFU 계열과 맞춘 VID/PID. dfu-util -d 1EAF:0003으로 찾는다. */
#define USB_VID            0x1EAFU
#define USB_PID            0x0003U
/* STM32F103 USB full-speed EP0의 일반적인 최대 패킷 크기. */
#define EP0_SIZE           64U
/* dfu-util이 한 번에 내려보내는 DFU 데이터 블록 크기. */
#define DFU_XFER_SIZE      1024U

/* STM32F103 USB PMA는 16비트 단위이며 워드 간격이 2칸처럼 보인다. */
#define PMA_ACCESS         2U
/* EP0 IN/OUT 버퍼를 USB PMA 안에 배치하는 주소. */
#define EP0_TX_ADDR        0x40U
#define EP0_RX_ADDR        0x80U

/* RX count register에서 32바이트 블록 모드를 표시하는 비트. */
#define USB_CNTRX_BLSIZE   0x8000U
/* STM32F103 중밀도 플래시는 1 KiB 페이지 단위로 erase한다. */
#define FLASH_PAGE_SIZE    1024U

/* USB 표준 control request 번호. enumeration 중 host가 EP0로 보낸다. */
#define USB_REQ_GET_STATUS        0x00U
#define USB_REQ_CLEAR_FEATURE     0x01U
#define USB_REQ_SET_ADDRESS       0x05U
#define USB_REQ_GET_DESCRIPTOR    0x06U
#define USB_REQ_GET_CONFIGURATION 0x08U
#define USB_REQ_SET_CONFIGURATION 0x09U
#define USB_REQ_GET_INTERFACE     0x0AU
#define USB_REQ_SET_INTERFACE     0x0BU

/* USB descriptor type 번호. GET_DESCRIPTOR 요청의 wValue 상위 바이트에 들어온다. */
#define USB_DESC_DEVICE           0x01U
#define USB_DESC_CONFIGURATION    0x02U
#define USB_DESC_STRING           0x03U

/* USB DFU class-specific request 번호. dfu-util과의 실제 업데이트 대화에 쓰인다. */
#define DFU_DETACH                0x00U
#define DFU_DNLOAD                0x01U
#define DFU_UPLOAD                0x02U
#define DFU_GETSTATUS             0x03U
#define DFU_CLRSTATUS             0x04U
#define DFU_GETSTATE              0x05U
#define DFU_ABORT                 0x06U

/* DFU 1.1 상태값. GETSTATUS/GETSTATE 응답에 그대로 실어 보낸다. */
#define DFU_STATE_DFU_IDLE        2U
#define DFU_STATE_DNLOAD_SYNC     3U
#define DFU_STATE_DNBUSY          4U
#define DFU_STATE_DNLOAD_IDLE     5U
#define DFU_STATE_MANIFEST_SYNC   6U
#define DFU_STATE_MANIFEST        7U
#define DFU_STATE_MANIFEST_RESET  8U
#define DFU_STATE_ERROR           10U

/* DFU status code. 주소 오류와 플래시 쓰기 오류만 최소 구현한다. */
#define DFU_STATUS_OK             0x00U
#define DFU_STATUS_ERR_ADDRESS    0x08U
#define DFU_STATUS_ERR_WRITE      0x03U

/* ST DfuSe 확장 명령. block 0 payload의 첫 바이트로 구분한다. */
#define DFUSE_CMD_SET_ADDRESS     0x21U
#define DFUSE_CMD_ERASE           0x41U

/* 앱 Reset_Handler로 점프하기 위한 함수 포인터 타입. */
typedef void (*entry_fn_t)(void);

/* USB SETUP 패킷 8바이트 구조. EP0 SETUP 수신 시 PMA에서 이 형태로 읽는다. */
typedef struct
{
  uint8_t bmRequestType;
  uint8_t bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
} setup_pkt_t;

/* SysTick에서 1 ms마다 증가하는 간단한 시간 기준. */
static volatile uint32_t g_ms;
/* 가장 최근에 받은 USB SETUP 패킷. */
static setup_pkt_t g_setup;
/* EP0 OUT으로 들어온 DFU 데이터 블록을 임시로 모으는 버퍼. */
static uint8_t g_ep0_out[DFU_XFER_SIZE];
/* EP0 IN 전송이 64바이트를 넘을 때 남은 데이터 위치와 길이. */
static const uint8_t *g_tx_ptr;
static uint16_t g_tx_len;
/* SET_ADDRESS는 상태 단계가 끝난 뒤 적용해야 하므로 잠시 보관한다. */
static uint8_t g_pending_addr;
/* SET_CONFIGURATION으로 받은 현재 configuration 값. */
static uint8_t g_configured;
/* 현재 EP0 OUT 데이터 단계를 기다리는 중인지 표시한다. */
static uint8_t g_expect_out;
/* 이번 EP0 OUT 전송에서 받아야 하는 총 길이와 현재 누적 길이. */
static uint16_t g_ep0_expected;
static uint16_t g_ep0_received;
/* DFU 상태 머신의 현재 상태와 오류 코드. */
static uint8_t g_dfu_state = DFU_STATE_DFU_IDLE;
static uint8_t g_dfu_status = DFU_STATUS_OK;
/* erase/write 직후 dfu-util에 한 번 DNBUSY 상태를 보여주기 위한 플래그. */
static uint8_t g_dfu_busy_once;
/* DFU manifest/leave 이후 리셋을 예약하기 위한 플래그와 시각. */
static uint8_t g_leave_requested;
static uint32_t g_leave_at;
/* DfuSe SET_ADDRESS 명령으로 설정되는 현재 플래시 기록 기준 주소. */
static uint32_t g_dfu_address = APP_ADDRESS;
/* DFU 다운로드 활동이 한 번이라도 있었는지 표시한다. 앱 자동 점프 억제에 사용한다. */
static uint32_t g_activity_seen;

/* USB device descriptor. PC가 처음 장치를 인식할 때 읽는다. */
static const uint8_t dev_desc[] =
{
  18, USB_DESC_DEVICE, 0x00, 0x02, 0x00, 0x00, 0x00, EP0_SIZE,
  (uint8_t)USB_VID, (uint8_t)(USB_VID >> 8),
  (uint8_t)USB_PID, (uint8_t)(USB_PID >> 8),
  0x00, 0x02, 1, 2, 3, 1
};

/* USB configuration/interface/DFU functional descriptor를 한 번에 담는다. */
static const uint8_t cfg_desc[] =
{
  9, USB_DESC_CONFIGURATION, 27, 0, 1, 1, 0, 0x80, 50,
  9, 4, 0, 0, 0, 0xFE, 0x01, 0x02, 4,
  9, 0x21, 0x0B, 0xFF, 0x00,
  (uint8_t)DFU_XFER_SIZE, (uint8_t)(DFU_XFER_SIZE >> 8), 0x10, 0x01
};

/* 문자열 descriptor들. str4는 DfuSe 메모리 맵 문자열이다. */
static const uint8_t str0[] = {4, USB_DESC_STRING, 0x09, 0x04};
static const uint8_t str1[] = {18, USB_DESC_STRING, 'h',0,'a',0,'e',0,'w',0,'o',0,'o',0,'n',0,'g',0};
static const uint8_t str2[] = {38, USB_DESC_STRING, 'M',0,'i',0,'n',0,' ',0,'D',0,'F',0,'U',0,' ',0,'B',0,'o',0,'o',0,'t',0,'l',0,'o',0,'a',0,'d',0,'e',0,'r',0};
static const uint8_t str3[] = {26, USB_DESC_STRING, '0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'1',0};
static const uint8_t str4[] =
{
  76, USB_DESC_STRING,
  '@',0,'I',0,'n',0,'t',0,'e',0,'r',0,'n',0,'a',0,'l',0,' ',0,'F',0,'l',0,'a',0,'s',0,'h',0,' ',0,
  '/',0,'0',0,'x',0,'0',0,'8',0,'0',0,'0',0,'2',0,'0',0,'0',0,'0',0,'/',0,
  '1',0,'2',0,'0',0,'*',0,'0',0,'0',0,'1',0,'K',0,'g',0
};

static void clock_init(void);
static void systick_init(void);
static void usb_gpio_disconnect(void);
static void usb_init(void);
static void usb_reset(void);
static void usb_handle_ctr(void);
static void usb_setup(void);
static void usb_rx_out(void);
static void usb_tx_done(void);
static void ep0_send(const uint8_t *data, uint16_t len);
static void ep0_send_zlp(void);
static void ep0_stall(void);
static void ep0_rx_valid(void);
static void ep0_tx_status(uint16_t status);
static void ep0_rx_status(uint16_t status);
static void pma_write(uint16_t pma_addr, const uint8_t *src, uint16_t len);
static void pma_read(uint16_t pma_addr, uint8_t *dst, uint16_t len);
static void pma_set_u16(uint16_t offset, uint16_t value);
static uint16_t pma_get_u16(uint16_t offset);
static void handle_standard(void);
static void handle_dfu(void);
static void dfu_process_dnload(const uint8_t *buf, uint16_t len);
static void flash_unlock(void);
static void flash_lock(void);
static uint8_t flash_erase_page(uint32_t addr);
static uint8_t flash_write(uint32_t addr, const uint8_t *src, uint16_t len);
static uint8_t address_allowed(uint32_t addr, uint32_t len);
static void flash_wait(void);
static void flash_clear_flags(void);
static uint8_t app_valid(void);
static void jump_app(void);
static uint16_t min16(uint16_t a, uint16_t b);
static uint32_t rd32(const uint8_t *p);

void SystemInit(void) {}
void __libc_init_array(void) {}

int main(void)
{
  /* USB는 48 MHz 클럭이 정확해야 하므로 먼저 72 MHz 시스템 클럭을 만든다. */
  clock_init();
  /* 1 ms 타이머를 만들어 부트 타임아웃과 leave 리셋 지연에 사용한다. */
  systick_init();
  /* D+를 잠깐 low로 당겨 PC가 USB 재연결로 인식하게 만든다. */
  usb_gpio_disconnect();
  /* STM32F103 USB device peripheral과 EP0 interrupt를 켠다. */
  usb_init();

  /*
   * 이 루프는 별도 scheduler 없이 두 가지 조건만 계속 감시한다.
   * - DFU 활동이 없으면 일정 시간 뒤 앱으로 점프
   * - DFU leave/reset 요청이 들어오면 약간 기다린 뒤 MCU 리셋
   */
  while (1)
  {
    /* 디버그 고정 모드가 아니고, DFU 활동이 없고, 1초가 지났고, 앱이 유효하면 실행한다. */
    if ((DEBUG_STAY_IN_DFU == 0U) && (g_activity_seen == 0U) && (g_ms > BOOT_TIMEOUT_MS) && app_valid())
    {
      jump_app();
    }
    /* dfu-util이 :leave를 사용했을 때는 status 응답 후 약간 기다렸다가 리셋한다. */
    if ((g_leave_requested != 0U) && ((g_ms - g_leave_at) > 100U))
    {
      NVIC_SystemReset();
    }
  }
}

void SysTick_Handler(void)
{
  /* 72 MHz / 72000 = 1 kHz이므로 1 ms마다 증가한다. */
  g_ms++;
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
  uint16_t istr = USB->ISTR;

  /* USB reset은 enumeration 시작 시 PC가 보낸다. EP0 상태를 처음부터 다시 잡는다. */
  if ((istr & USB_ISTR_RESET) != 0U)
  {
    USB->ISTR = (uint16_t)~USB_ISTR_RESET;
    usb_reset();
  }

  /* CTR은 endpoint 전송 완료 이벤트다. pending 이벤트가 없을 때까지 처리한다. */
  while ((USB->ISTR & USB_ISTR_CTR) != 0U)
  {
    usb_handle_ctr();
  }
}

static void clock_init(void)
{
  /* 플래시 prefetch와 wait state를 72 MHz 동작에 맞춘다. */
  FLASH->ACR |= FLASH_ACR_PRFTBE; // prefetch enable (다음 명령어를 미리 읽어두는 기능)
  FLASH->ACR &= ~FLASH_ACR_LATENCY;
  FLASH->ACR |= FLASH_ACR_LATENCY_2;  // 2 wait state (72 MHz에서 플래시 읽기 지연을 맞추기 위해 필요)

  /* 외부 8 MHz HSE를 켠다. Blue Pill 계열 보드는 보통 8 MHz 크리스털을 쓴다. */
  RCC->CR |= RCC_CR_HSEON;
  while ((RCC->CR & RCC_CR_HSERDY) == 0U) {}

  /* HSE x9 = 72 MHz, APB1은 36 MHz, USB는 PLL/1.5 = 48 MHz가 되게 설정한다. */
  RCC->CFGR = 0U;
  RCC->CFGR |= RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
  RCC->CFGR &= ~RCC_CFGR_USBPRE;

  /* PLL을 켜고 시스템 클럭 소스로 선택한다. */
  RCC->CR |= RCC_CR_PLLON;
  while ((RCC->CR & RCC_CR_PLLRDY) == 0U) {}

  RCC->CFGR |= RCC_CFGR_SW_PLL; // PLL을 시스템 클럭으로 선택
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {} // 실제 사용중인 클럭소스가 PLL인지 확인
}

static void systick_init(void)
{
  /* 72 MHz에서 72000 tick마다 interrupt를 걸면 1 ms 주기가 된다. */
  SysTick->LOAD = 72000U - 1U;
  SysTick->VAL = 0U;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

static void usb_gpio_disconnect(void)
{
  /*
   * 많은 STM32F103 보드는 USB D+에 pull-up이 고정되어 있다.
   * PA12(D+)를 잠깐 output low로 만들어 PC가 물리적인 disconnect/reconnect를 본 것처럼 만든다.
   */
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; // GPIOA clock enable
  GPIOA->CRH &= ~(GPIO_CRH_MODE12 | GPIO_CRH_CNF12);  // 설정비트 초기화
  GPIOA->CRH |= GPIO_CRH_MODE12_1;  // PA12 output 설정
  GPIOA->BRR = GPIO_BRR_BR12; // PA12 Low 출력
  for (volatile uint32_t i = 0; i < 720000U; i++) {}  // 72MHz기준, 약 10 ms
  GPIOA->CRH &= ~(GPIO_CRH_MODE12 | GPIO_CRH_CNF12);
  GPIOA->CRH |= GPIO_CRH_CNF12_0;
}

static void usb_init(void)
{
  /* USB peripheral 클럭을 켜고 force reset으로 내부 상태를 초기화한다. */
  RCC->APB1ENR |= RCC_APB1ENR_USBEN;
  USB->CNTR = USB_CNTR_FRES;
  USB->CNTR = 0U;
  USB->ISTR = 0U;
  USB->BTABLE = 0U;
  /* USB reset 이벤트와 endpoint CTR 이벤트만 interrupt로 받는다. */
  USB->CNTR = USB_CNTR_RESETM | USB_CNTR_CTRM;
  USB->DADDR = USB_DADDR_EF;
  NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 0U);
  NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}

static void usb_reset(void)
{
  /* BTABLE은 PMA 시작 주소 0에 둔다. EP0 descriptor 4개 word를 직접 설정한다. */
  USB->BTABLE = 0U;
  /* EP0 TX buffer address, TX count, RX buffer address, RX count 설정. */
  pma_set_u16(0U, EP0_TX_ADDR);
  pma_set_u16(2U, 0U);
  pma_set_u16(4U, EP0_RX_ADDR);
  /* 64바이트 RX 버퍼 설정. 0x8400은 STM32 USB PMA count 형식이다. */
  pma_set_u16(6U, 0x8400U);

  /* EP0를 control endpoint로 열고, 먼저 OUT/SETUP을 받을 준비를 한다. */
  USB->EP0R = USB_EP_CONTROL | USB_EP_CTR_RX | USB_EP_CTR_TX;
  ep0_tx_status(USB_EP_TX_NAK);
  ep0_rx_status(USB_EP_RX_VALID);

  /* USB 주소와 EP0/DFU 상태를 enumeration 시작 상태로 되돌린다. */
  USB->DADDR = USB_DADDR_EF;
  g_pending_addr = 0U;
  g_configured = 0U;
  g_expect_out = 0U;
  g_tx_len = 0U;
  g_dfu_state = DFU_STATE_DFU_IDLE;
}

static void usb_handle_ctr(void)
{
  uint16_t ep = USB->ISTR & USB_ISTR_EP_ID;
  uint16_t epr;

  /* 이 부트로더는 EP0만 구현한다. 다른 endpoint 이벤트는 무시한다. */
  if (ep != 0U)
  {
    USB->ISTR = (uint16_t)~USB_ISTR_CTR;
    return;
  }

  epr = USB->EP0R;
  if ((epr & USB_EP_CTR_RX) != 0U)
  {
    /* RX 완료 플래그를 지우되 TX 완료 플래그는 보존한다. */
    USB->EP0R = (uint16_t)((epr & (0x7FFFU & USB_EPREG_MASK)) | USB_EP_CTR_TX);
    if ((epr & USB_EP_SETUP) != 0U)
    {
      /* SETUP 단계: 표준 USB 요청 또는 DFU class 요청을 해석한다. */
      usb_setup();
    }
    else
    {
      /* OUT 데이터 단계: DFU 다운로드 payload를 이어 받는다. */
      usb_rx_out();
    }
  }

  epr = USB->EP0R;
  if ((epr & USB_EP_CTR_TX) != 0U)
  {
    /* TX 완료 플래그를 지우되 RX 완료 플래그는 보존한다. */
    USB->EP0R = (uint16_t)((epr & (0xFF7FU & USB_EPREG_MASK)) | USB_EP_CTR_RX);
    usb_tx_done();
  }
}

static void usb_setup(void)
{
  /* EP0 RX PMA에는 SETUP 패킷 8바이트가 들어 있다. */
  pma_read(EP0_RX_ADDR, (uint8_t *)&g_setup, 8U);
  g_tx_ptr = 0;
  g_tx_len = 0U;
  g_expect_out = 0U;
  ep0_tx_status(USB_EP_TX_NAK);

  /* bmRequestType type 필드가 class(0x20)이면 DFU 요청으로 본다. */
  if ((g_setup.bmRequestType & 0x60U) == 0x20U)
  {
    handle_dfu();
  }
  else
  {
    handle_standard();
  }
}

static void handle_standard(void)
{
  /* GET_DESCRIPTOR의 상위 바이트는 descriptor type, 하위 바이트는 index다. */
  uint8_t dtype = (uint8_t)(g_setup.wValue >> 8);
  uint8_t dindex = (uint8_t)g_setup.wValue;
  static uint8_t two[2];
  static uint8_t one[1];
  const uint8_t *p = 0;
  uint16_t len = 0U;

  switch (g_setup.bRequest)
  {
    case USB_REQ_GET_DESCRIPTOR:
      /* PC가 장치/구성/문자열 descriptor를 요청하면 준비된 배열을 돌려준다. */
      if (dtype == USB_DESC_DEVICE) { p = dev_desc; len = sizeof(dev_desc); }
      else if (dtype == USB_DESC_CONFIGURATION) { p = cfg_desc; len = sizeof(cfg_desc); }
      else if (dtype == USB_DESC_STRING)
      {
        if (dindex == 0U) { p = str0; len = sizeof(str0); }
        else if (dindex == 1U) { p = str1; len = sizeof(str1); }
        else if (dindex == 2U) { p = str2; len = sizeof(str2); }
        else if (dindex == 3U) { p = str3; len = sizeof(str3); }
        else if (dindex == 4U) { p = str4; len = sizeof(str4); }
      }
      if (p != 0) { ep0_send(p, min16(len, g_setup.wLength)); }
      else { ep0_stall(); }
      break;

    case USB_REQ_SET_ADDRESS:
      /*
       * USB 주소는 status stage가 끝난 뒤 적용해야 한다.
       * 그래서 여기서는 g_pending_addr에 저장하고 ZLP 응답 후 usb_tx_done에서 적용한다.
       */
      g_pending_addr = (uint8_t)(g_setup.wValue & 0x7FU);
      ep0_send_zlp();
      break;

    case USB_REQ_SET_CONFIGURATION:
      /* DFU interface 하나뿐이므로 configuration 값만 기억하고 성공 응답한다. */
      g_configured = (uint8_t)g_setup.wValue;
      ep0_send_zlp();
      break;

    case USB_REQ_GET_CONFIGURATION:
      /* 현재 configuration 값을 1바이트로 반환한다. */
      one[0] = g_configured;
      ep0_send(one, 1U);
      break;

    case USB_REQ_GET_INTERFACE:
      /* alternate setting은 0 하나만 사용한다. */
      one[0] = 0U;
      ep0_send(one, 1U);
      break;

    case USB_REQ_GET_STATUS:
      /* self-powered/remote-wakeup 같은 상태 비트를 모두 0으로 응답한다. */
      two[0] = 0U; two[1] = 0U;
      ep0_send(two, 2U);
      break;

    case USB_REQ_SET_INTERFACE:
    case USB_REQ_CLEAR_FEATURE:
      /* 이 최소 구현에서는 별도 동작 없이 성공 처리한다. */
      ep0_send_zlp();
      break;

    default:
      /* 지원하지 않는 표준 요청은 STALL로 거절한다. */
      ep0_stall();
      break;
  }
}

static void handle_dfu(void)
{
  static uint8_t resp[6];

  switch (g_setup.bRequest)
  {
    case DFU_DNLOAD:
      /*
       * dfu-util이 펌웨어를 내려보낼 때 사용하는 핵심 요청.
       * wLength가 0이면 다운로드 종료(manifest), 0보다 크면 OUT data stage가 이어진다.
       */
      g_activity_seen = 1U;
      if (g_setup.wLength == 0U)
      {
        /* 0-length DNLOAD는 전송 종료 신호다. 이후 GETSTATUS에서 reset 상태로 넘어간다. */
        g_dfu_state = DFU_STATE_MANIFEST_SYNC;
        ep0_send_zlp();
      }
      else if (g_setup.wLength <= DFU_XFER_SIZE)
      {
        /* 실제 데이터는 다음 OUT transaction으로 들어오므로 받을 길이를 기록해둔다. */
        g_expect_out = 1U;
        g_ep0_expected = g_setup.wLength;
        g_ep0_received = 0U;
        ep0_rx_valid();
      }
      else
      {
        /* 이 부트로더가 받도록 정한 1024바이트보다 큰 블록은 거절한다. */
        ep0_stall();
      }
      break;

    case DFU_GETSTATUS:
    {
      /*
       * dfu-util은 DNLOAD 후 GETSTATUS를 반복 호출하며 장치가 busy인지 idle인지 확인한다.
       * 플래시 erase/write 직후에는 한 번 DNBUSY를 보여주고 다음에는 DNLOAD_IDLE로 간다.
       */
      uint8_t response_state = g_dfu_state;

      if (g_dfu_state == DFU_STATE_DNLOAD_SYNC)
      {
        if (g_dfu_busy_once != 0U)
        {
          /* 긴 작업이 있었다는 뜻을 host에 알려주기 위한 1회성 busy 응답. */
          response_state = DFU_STATE_DNBUSY;
          g_dfu_busy_once = 0U;
          g_dfu_state = DFU_STATE_DNLOAD_IDLE;
        }
        else
        {
          g_dfu_state = DFU_STATE_DNLOAD_IDLE;
          response_state = g_dfu_state;
        }
      }
      else if (g_dfu_state == DFU_STATE_MANIFEST_SYNC)
      {
        /* 다운로드가 끝났으므로 host에 reset 예정 상태를 알리고 실제 reset을 예약한다. */
        g_dfu_state = DFU_STATE_MANIFEST_RESET;
        response_state = g_dfu_state;
        g_leave_at = g_ms;
        g_leave_requested = 1U;
      }

      /* DFU GETSTATUS 응답: status 1바이트, poll timeout 3바이트, state 1바이트, iString 1바이트. */
      resp[0] = g_dfu_status;
      resp[1] = 1U; resp[2] = 0U; resp[3] = 0U;
      resp[4] = response_state;
      resp[5] = 0U;
      ep0_send(resp, 6U);
      break;
    }

    case DFU_GETSTATE:
      /* 현재 DFU 상태만 1바이트로 반환한다. */
      resp[0] = g_dfu_state;
      ep0_send(resp, 1U);
      break;

    case DFU_CLRSTATUS:
    case DFU_ABORT:
      /* 오류 상태를 지우거나 현재 다운로드를 중단하고 idle로 돌아간다. */
      g_dfu_status = DFU_STATUS_OK;
      g_dfu_state = DFU_STATE_DFU_IDLE;
      ep0_send_zlp();
      break;

    case DFU_DETACH:
      /* runtime DFU detach는 사용하지 않지만 성공 응답만 해서 host를 막지 않는다. */
      ep0_send_zlp();
      break;

    default:
      /* UPLOAD 등 지원하지 않는 DFU 요청은 STALL 처리한다. */
      ep0_stall();
      break;
  }
}

static void usb_rx_out(void)
{
  /* EP0 RX count register에서 이번 OUT packet 길이만 꺼낸다. */
  uint16_t len = pma_get_u16(6U) & 0x03FFU;

  /*
   * DFU_DNLOAD가 먼저 와서 OUT data stage를 기대하는 상태여야 한다.
   * EP0 packet은 최대 64바이트이므로 여러 packet을 모아 DFU_XFER_SIZE 블록을 만든다.
   */
  if ((g_expect_out != 0U) && (len <= EP0_SIZE) && ((g_ep0_received + len) <= DFU_XFER_SIZE))
  {
    pma_read(EP0_RX_ADDR, &g_ep0_out[g_ep0_received], len);
    g_ep0_received += len;

    if (g_ep0_received >= g_ep0_expected)
    {
      /* 이번 DFU block을 모두 받았으므로 DfuSe 명령 또는 플래시 데이터로 처리한다. */
      g_expect_out = 0U;
      dfu_process_dnload(g_ep0_out, g_ep0_received);
      ep0_send_zlp();
    }
    else
    {
      /* 아직 block이 끝나지 않았으므로 다음 OUT packet을 계속 받는다. */
      ep0_rx_valid();
    }
  }
  else
  {
    /* 예상하지 않은 OUT data나 너무 큰 packet은 control transfer 오류로 처리한다. */
    g_expect_out = 0U;
    ep0_stall();
  }
}

static void dfu_process_dnload(const uint8_t *buf, uint16_t len)
{
  uint32_t addr;

  /* 이미 DFU 오류 상태라면 새 데이터를 처리하지 않는다. host가 CLRSTATUS를 보내야 회복된다. */
  if (g_dfu_status != DFU_STATUS_OK)
  {
    return;
  }

  if (g_setup.wValue == 0U)
  {
    /*
     * DfuSe 프로토콜에서 block number 0은 명령 블록이다.
     * dfu-util은 여기로 SET_ADDRESS 또는 ERASE 명령을 먼저 보낸다.
     */
    if ((len >= 5U) && (buf[0] == DFUSE_CMD_SET_ADDRESS))
    {
      /* 이후 데이터 block이 기록될 기준 주소를 host가 지정한다. */
      addr = rd32(&buf[1]);
      if (address_allowed(addr, 1U) != 0U)
      {
        g_dfu_address = addr;
        g_dfu_state = DFU_STATE_DNLOAD_SYNC;
        g_dfu_busy_once = 1U;
      }
      else
      {
        /* 앱 영역 밖 주소를 요구하면 부트로더 보호를 위해 오류 상태로 간다. */
        g_dfu_status = DFU_STATUS_ERR_ADDRESS;
        g_dfu_state = DFU_STATE_ERROR;
      }
    }
    else if ((len >= 5U) && (buf[0] == DFUSE_CMD_ERASE))
    {
      /* host가 지정한 주소가 속한 1 KiB 플래시 페이지를 지운다. */
      addr = rd32(&buf[1]);
      if ((address_allowed(addr, FLASH_PAGE_SIZE) != 0U) && (flash_erase_page(addr) != 0U))
      {
        g_dfu_state = DFU_STATE_DNLOAD_SYNC;
        g_dfu_busy_once = 1U;
      }
      else
      {
        /* erase 실패는 write 오류로 보고 host에 알린다. */
        g_dfu_status = DFU_STATUS_ERR_WRITE;
        g_dfu_state = DFU_STATE_ERROR;
      }
    }
    else
    {
      /* 알 수 없는 block 0 명령은 최소 호환성을 위해 일단 성공 동기화로 넘긴다. */
      g_dfu_state = DFU_STATE_DNLOAD_SYNC;
    }
  }
  else if (g_setup.wValue >= 2U)
  {
    /*
     * DfuSe에서 실제 데이터 block은 block number 2부터 시작한다.
     * 주소 = SET_ADDRESS 기준 + (block number - 2) * transfer size.
     */
    addr = g_dfu_address + ((uint32_t)(g_setup.wValue - 2U) * DFU_XFER_SIZE);
    if ((address_allowed(addr, len) != 0U) && (flash_write(addr, buf, len) != 0U))
    {
      g_dfu_state = DFU_STATE_DNLOAD_SYNC;
    }
    else
    {
      /* 범위 밖 기록이나 플래시 기록 실패는 DFU error 상태로 고정한다. */
      g_dfu_status = DFU_STATUS_ERR_WRITE;
      g_dfu_state = DFU_STATE_ERROR;
    }
  }
  else
  {
    /* block number 1은 이 최소 구현에서 특별히 쓰지 않는다. */
    g_dfu_state = DFU_STATE_DNLOAD_SYNC;
  }
}

static void usb_tx_done(void)
{
  if (g_tx_len != 0U)
  {
    /* descriptor처럼 64바이트보다 긴 IN 응답은 EP0_SIZE 단위로 나눠서 계속 보낸다. */
    uint16_t chunk = min16(g_tx_len, EP0_SIZE);
    pma_write(EP0_TX_ADDR, g_tx_ptr, chunk);
    pma_set_u16(2U, chunk);
    g_tx_ptr += chunk;
    g_tx_len -= chunk;
    ep0_tx_status(USB_EP_TX_VALID);
  }
  else
  {
    /* SET_ADDRESS는 status stage가 끝난 지금 적용해야 USB 규격에 맞다. */
    if (g_pending_addr != 0U)
    {
      USB->DADDR = USB_DADDR_EF | g_pending_addr;
      g_pending_addr = 0U;
    }
    /* 다음 SETUP/OUT packet을 받을 수 있도록 EP0 RX를 다시 valid로 만든다. */
    ep0_rx_valid();
  }
}

static void ep0_send(const uint8_t *data, uint16_t len)
{
  /* 첫 packet을 PMA에 올리고, 남은 데이터는 usb_tx_done에서 이어 보낸다. */
  uint16_t chunk = min16(len, EP0_SIZE);
  g_tx_ptr = data + chunk;
  g_tx_len = len - chunk;
  pma_write(EP0_TX_ADDR, data, chunk);
  pma_set_u16(2U, chunk);
  ep0_rx_status(USB_EP_RX_VALID);
  ep0_tx_status(USB_EP_TX_VALID);
}

static void ep0_send_zlp(void)
{
  /* Zero Length Packet. 성공했지만 돌려줄 payload가 없을 때 status stage로 보낸다. */
  g_tx_ptr = 0;
  g_tx_len = 0U;
  pma_set_u16(2U, 0U);
  ep0_tx_status(USB_EP_TX_VALID);
  ep0_rx_status(USB_EP_RX_VALID);
}

static void ep0_stall(void)
{
  /* 지원하지 않거나 잘못된 control request는 양방향을 STALL로 막는다. */
  ep0_tx_status(USB_EP_TX_STALL);
  ep0_rx_status(USB_EP_RX_STALL);
}

static void ep0_rx_valid(void)
{
  /* host가 다음 OUT/SETUP packet을 보낼 수 있게 RX endpoint 상태를 VALID로 바꾼다. */
  ep0_rx_status(USB_EP_RX_VALID);
}

static void ep0_tx_status(uint16_t status)
{
  /*
   * STM32 USB EP register의 STAT_TX/STAT_RX 비트는 write-toggle 방식이다.
   * 원하는 상태와 현재 상태의 차이만 XOR로 토글해야 한다.
   */
  uint16_t r = USB->EP0R & USB_EPTX_DTOGMASK;
  if ((status & 0x0010U) != 0U) { r ^= 0x0010U; }
  if ((status & 0x0020U) != 0U) { r ^= 0x0020U; }
  USB->EP0R = r | USB_EP_CTR_RX | USB_EP_CTR_TX;
}

static void ep0_rx_status(uint16_t status)
{
  /* RX 상태도 TX와 같은 toggle 규칙을 따른다. */
  uint16_t r = USB->EP0R & USB_EPRX_DTOGMASK;
  if ((status & 0x1000U) != 0U) { r ^= 0x1000U; }
  if ((status & 0x2000U) != 0U) { r ^= 0x2000U; }
  USB->EP0R = r | USB_EP_CTR_RX | USB_EP_CTR_TX;
}

static void pma_write(uint16_t pma_addr, const uint8_t *src, uint16_t len)
{
  /*
   * USB PMA(packet memory area)에 byte 배열을 16비트 word로 복사한다.
   * F103 PMA는 일반 SRAM처럼 byte 접근하지 않고 전용 16비트 배치를 쓴다.
   */
  __IO uint16_t *p = (__IO uint16_t *)((uint32_t)USB + 0x400U + ((uint32_t)pma_addr * PMA_ACCESS));
  uint16_t i = 0U;
  while (i < len)
  {
    uint16_t v = src[i];
    if ((i + 1U) < len) { v |= ((uint16_t)src[i + 1U] << 8); }
    *p = v;
    p += PMA_ACCESS;
    i += 2U;
  }
}

static void pma_read(uint16_t pma_addr, uint8_t *dst, uint16_t len)
{
  /* PMA의 16비트 word를 다시 일반 byte 배열로 풀어서 읽는다. */
  __IO uint16_t *p = (__IO uint16_t *)((uint32_t)USB + 0x400U + ((uint32_t)pma_addr * PMA_ACCESS));
  uint16_t i = 0U;
  while (i < len)
  {
    uint16_t v = *p;
    dst[i] = (uint8_t)v;
    if ((i + 1U) < len) { dst[i + 1U] = (uint8_t)(v >> 8); }
    p += PMA_ACCESS;
    i += 2U;
  }
}

static void pma_set_u16(uint16_t offset, uint16_t value)
{
  /* BTABLE entry나 count register처럼 PMA 안의 16비트 값을 직접 쓴다. */
  *(__IO uint16_t *)((uint32_t)USB + 0x400U + ((uint32_t)offset * PMA_ACCESS)) = value;
}

static uint16_t pma_get_u16(uint16_t offset)
{
  /* BTABLE entry나 count register처럼 PMA 안의 16비트 값을 직접 읽는다. */
  return *(__IO uint16_t *)((uint32_t)USB + 0x400U + ((uint32_t)offset * PMA_ACCESS));
}

static void flash_unlock(void)
{
  /* STM32 내부 플래시는 lock 상태가 기본이므로 KEY1/KEY2를 순서대로 써서 해제한다. */
  if ((FLASH->CR & FLASH_CR_LOCK) != 0U)
  {
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
  }
}

static void flash_lock(void)
{
  /* erase/write가 끝나면 다시 lock해서 실수로 플래시가 바뀌지 않게 한다. */
  FLASH->CR |= FLASH_CR_LOCK;
}

static void flash_wait(void)
{
  /* 플래시 erase/write 진행 중에는 BSY가 내려갈 때까지 기다린다. */
  while ((FLASH->SR & FLASH_SR_BSY) != 0U) {}
}

static void flash_clear_flags(void)
{
  /* 이전 작업의 완료/오류 플래그를 지워 새 작업 결과만 판단한다. */
  FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
}

static uint8_t flash_erase_page(uint32_t addr)
{
  /* 어떤 주소가 들어와도 그 주소가 속한 1 KiB 페이지 시작 주소로 맞춘다. */
  addr -= (addr % FLASH_PAGE_SIZE);
  /* 페이지 erase 시퀀스: unlock -> wait -> flag clear -> PER -> AR -> STRT. */
  flash_unlock();
  flash_wait();
  flash_clear_flags();
  FLASH->CR |= FLASH_CR_PER;
  FLASH->AR = addr;
  FLASH->CR |= FLASH_CR_STRT;
  flash_wait();
  FLASH->CR &= ~FLASH_CR_PER;
  flash_lock();
  /* 프로그래밍/쓰기 보호 오류가 없으면 성공. */
  return ((FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) == 0U) ? 1U : 0U;
}

static uint8_t flash_write(uint32_t addr, const uint8_t *src, uint16_t len)
{
  uint16_t i = 0U;
  /*
   * STM32F1 플래시는 half-word(16비트) 단위로 program한다.
   * dfu-util 데이터 길이가 홀수일 수 있으므로 마지막 상위 바이트는 0xFF로 채운다.
   */
  flash_unlock();
  while (i < len)
  {
    uint16_t v = src[i];
    if ((i + 1U) < len) { v |= ((uint16_t)src[i + 1U] << 8); }
    else { v |= 0xFF00U; }
    flash_wait();
    flash_clear_flags();
    /* PG 비트를 켠 뒤 대상 주소에 half-word를 쓰면 플래시 program이 시작된다. */
    FLASH->CR |= FLASH_CR_PG;
    *(__IO uint16_t *)(addr + i) = v;
    flash_wait();
    FLASH->CR &= ~FLASH_CR_PG;
    /* 오류 플래그 또는 read-back 불일치가 있으면 실패로 처리한다. */
    if (((FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0U) || (*(__IO uint16_t *)(addr + i) != v))
    {
      flash_lock();
      return 0U;
    }
    i += 2U;
  }
  flash_lock();
  return 1U;
}

static uint8_t address_allowed(uint32_t addr, uint32_t len)
{
  /* 앱 시작 주소보다 낮은 쓰기는 부트로더 영역을 덮을 수 있으므로 금지한다. */
  if (addr < APP_ADDRESS) { return 0U; }
  /* 플래시 끝을 넘어가는 시작 주소도 금지한다. */
  if (addr >= FLASH_END_ADDRESS) { return 0U; }
  /* 시작 주소는 정상이어도 길이까지 더했을 때 플래시 끝을 넘으면 금지한다. */
  if (len > (FLASH_END_ADDRESS - addr)) { return 0U; }
  return 1U;
}

static uint8_t app_valid(void)
{
  /*
   * Cortex-M 벡터 테이블의 첫 word는 초기 SP, 두 번째 word는 Reset_Handler다.
   * SP가 SRAM 범위이고 PC가 앱 플래시 범위이면 앱이 있다고 판단한다.
   */
  uint32_t sp = *(__IO uint32_t *)APP_ADDRESS;
  uint32_t pc = *(__IO uint32_t *)(APP_ADDRESS + 4U);
  return ((sp >= SRAM_START) && (sp <= SRAM_END) && (pc >= APP_ADDRESS) && (pc < FLASH_END_ADDRESS)) ? 1U : 0U;
}

static void jump_app(void)
{
  /* 앱 벡터 테이블에서 초기 stack pointer와 reset handler 주소를 가져온다. */
  uint32_t sp = *(__IO uint32_t *)APP_ADDRESS;
  uint32_t pc = *(__IO uint32_t *)(APP_ADDRESS + 4U);
  entry_fn_t app = (entry_fn_t)pc;
  /* 점프 중 interrupt가 끼어들지 않도록 잠깐 막는다. */
  __disable_irq();
  /* 부트로더가 쓰던 USB peripheral과 SysTick을 정리한다. */
  USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
  RCC->APB1ENR &= ~RCC_APB1ENR_USBEN;
  SysTick->CTRL = 0U;
  /* interrupt vector base를 앱 시작 주소로 바꾼다. */
  SCB->VTOR = APP_ADDRESS;
  /* MSP를 앱의 초기 stack pointer로 바꾼 뒤 앱 Reset_Handler를 호출한다. */
  __set_MSP(sp);
  __enable_irq();
  app();
}


static uint16_t min16(uint16_t a, uint16_t b)
{
  /* EP0 packet 나누기용 작은 min 함수. 표준 라이브러리 의존을 줄이기 위해 직접 둔다. */
  return (a < b) ? a : b;
}

static uint32_t rd32(const uint8_t *p)
{
  /* DfuSe 명령 payload의 little-endian 32비트 주소를 읽는다. */
  return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
