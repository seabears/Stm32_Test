ENTRY(Reset_Handler)

/*
 * Minimal DFU bootloader 전용 링커 스크립트.
 *
 * 목표:
 *   - 부트로더는 플래시 시작 주소 0x08000000에서 실행된다.
 *   - 크기는 8 KiB 안에 들어가야 한다.
 *   - 사용자 앱은 0x08002000부터 시작하므로 이 스크립트는 앱 영역을 사용하지 않는다.
 */

/* Cortex-M3 reset 직후 MSP에 들어갈 스택 최상단 주소. */
_estack = ORIGIN(RAM) + LENGTH(RAM);
/* 이 부트로더는 malloc을 쓰지 않으므로 heap은 0으로 둔다. */
_Min_Heap_Size = 0x0;
/* 링커가 최소 1 KiB stack 공간을 RAM에 예약하게 한다. */
_Min_Stack_Size = 0x400;

MEMORY
{
  /* STM32F103C8T6 SRAM 20 KiB. */
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
  /* 부트로더가 차지할 플래시 앞쪽 8 KiB만 링커에 허용한다. */
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 8K
}

SECTIONS
{
  /* 벡터 테이블은 반드시 플래시 시작에 있어야 reset 후 CPU가 올바르게 진입한다. */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* 실행 코드와 const 데이터는 부트로더 플래시 영역에 배치한다. */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  /* .data의 초기값은 플래시에 있고, 실행 시 startup code가 RAM으로 복사한다. */
  _sidata = LOADADDR(.data);

  /* 초기값이 있는 전역/static 변수는 실행 주소가 RAM이다. */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  /* 초기값이 0인 전역/static 변수는 RAM에 배치하고 startup code가 0으로 지운다. */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* heap/stack 예약 영역. 실제 stack은 _estack에서 아래로 자란다. */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /*
   * 표준 C 라이브러리, math 라이브러리, 불필요한 libgcc 섹션을 버린다.
   * 8 KiB 목표를 위해 런타임 의존을 최대한 작게 유지한다.
   */
  /DISCARD/ :
  {
    libc.a(*)
    libm.a(*)
    libgcc.a(*)
  }

  /* ARM object metadata. 실행에는 필요 없지만 toolchain 호환을 위해 위치만 둔다. */
  .ARM.attributes 0 : { *(.ARM.attributes) }
}
