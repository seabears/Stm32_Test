#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*
 * FreeRTOS 커널의 기능과 동작 방식을 결정하는 프로젝트 설정 파일입니다.
 * FreeRTOS.h가 이 파일을 자동으로 포함하므로 애플리케이션에서 직접 include할
 * 필요는 없습니다. 이 파일의 값은 컴파일 시 커널 코드 구성에도 사용됩니다.
 *
 * 주의: Cortex-M에서 태스크 스택 크기의 단위는 byte가 아니라 StackType_t의
 * 개수(word)입니다. 이 프로젝트에서는 StackType_t가 32-bit(4bytes)이므로 128은
 * 128 words, 즉 512 bytes입니다.
 */

#if defined(__GNUC__)
#include <stdint.h>
/* STM32 시스템 초기화 코드가 갱신하는 실제 CPU 클럭 주파수입니다. */
extern uint32_t SystemCoreClock;
#endif

/* 스케줄러 기본 동작 ------------------------------------------------------- */
/* 1: 높은 우선순위 태스크가 준비되면 실행 중인 태스크를 즉시 선점합니다. */
#define configUSE_PREEMPTION                    1

/* Idle 태스크가 반복될 때 호출되는 vApplicationIdleHook()을 사용하지 않습니다. */
#define configUSE_IDLE_HOOK                     0

/* 매 RTOS tick마다 호출되는 vApplicationTickHook()을 사용하지 않습니다. */
#define configUSE_TICK_HOOK                     0

/* FreeRTOS 포트가 SysTick 등을 설정할 때 사용하는 CPU 클럭입니다. */
#define configCPU_CLOCK_HZ                      (SystemCoreClock)

/* RTOS tick 주파수 1 kHz: tick 한 번은 1 ms입니다. */
#define configTICK_RATE_HZ                      ((TickType_t)1000)

/* 사용할 수 있는 우선순위 개수입니다. 유효 범위는 0~4입니다. */
#define configMAX_PRIORITIES                    5

/* Idle 태스크 등에 사용하는 최소 스택: 128 words = 512 bytes입니다. */
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)

/* heap_4.c가 동적 객체/태스크 할당에 사용할 전용 힙 크기입니다. */
#define configTOTAL_HEAP_SIZE                   ((size_t)(6U * 1024U))

/* 태스크 이름의 최대 길이이며 문자열 종료 문자까지 포함합니다. */
#define configMAX_TASK_NAME_LEN                 16

/* 태스크 상태/통계 수집용 trace 기능을 사용하지 않습니다. */
#define configUSE_TRACE_FACILITY                0

/* 0: TickType_t를 32-bit로 사용합니다. 1이면 16-bit가 됩니다. */
#define configUSE_16_BIT_TICKS                  0

/* 같은 우선순위의 준비 태스크가 있으면 Idle 태스크가 CPU를 양보합니다. */
#define configIDLE_SHOULD_YIELD                 1

/* mutex와 우선순위 상속 기능을 사용합니다. */
#define configUSE_MUTEXES                       1

/* 디버거에서 queue 이름을 조회하는 registry를 만들지 않습니다. */
#define configQUEUE_REGISTRY_SIZE               0

/* 2: 태스크 전환 시와 ISR 종료 시 모두 스택 오버플로를 검사합니다. */
#define configCHECK_FOR_STACK_OVERFLOW          2

/* 재귀 mutex API는 사용하지 않습니다. */
#define configUSE_RECURSIVE_MUTEXES             0

/* 메모리 할당 실패 시 vApplicationMallocFailedHook()을 호출합니다. */
#define configUSE_MALLOC_FAILED_HOOK            1

/* 태스크별 application hook/tag 포인터 기능을 사용하지 않습니다. */
#define configUSE_APPLICATION_TASK_TAG          0

/* counting semaphore API는 사용하지 않습니다. */
#define configUSE_COUNTING_SEMAPHORES           0

/* 태스크별 CPU 실행 시간 통계를 수집하지 않습니다. */
#define configGENERATE_RUN_TIME_STATS           0

/* xTaskCreate() 등 FreeRTOS 힙을 사용하는 동적 할당 API를 허용합니다. */
#define configSUPPORT_DYNAMIC_ALLOCATION        1

/* xTaskCreateStatic() 등의 정적 할당 API는 사용하지 않습니다. */
#define configSUPPORT_STATIC_ALLOCATION         0

/* 구형 co-routine 기능 ----------------------------------------------------- */
/* 태스크보다 제약이 많은 구형 co-routine 기능을 사용하지 않습니다. */
#define configUSE_CO_ROUTINES                   0

/* co-routine을 켤 경우 사용할 우선순위 개수입니다(현재는 미사용). */
#define configMAX_CO_ROUTINE_PRIORITIES         1

/* 소프트웨어 타이머 -------------------------------------------------------- */
/* FreeRTOS software timer service 태스크를 만들지 않습니다. */
#define configUSE_TIMERS                        0

/* 아래 세 값은 configUSE_TIMERS가 1일 때만 사용됩니다. */
#define configTIMER_TASK_PRIORITY               2
#define configTIMER_QUEUE_LENGTH                5
#define configTIMER_TASK_STACK_DEPTH            128

/* 선택 API 포함 여부 ------------------------------------------------------- */
/* 0인 API는 커널 바이너리에서 제외되어 Flash 사용량을 줄입니다. */
#define INCLUDE_vTaskPrioritySet                0
#define INCLUDE_uxTaskPriorityGet               0
#define INCLUDE_vTaskDelete                     0
#define INCLUDE_vTaskCleanUpResources           0
#define INCLUDE_vTaskSuspend                    0
/* 주기 태스크와 일반 지연에 사용하므로 delay API는 포함합니다. */
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xQueueGetMutexHolder            0
/* SysTick ISR에서 스케줄러 시작 여부를 확인하므로 포함합니다. */
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_eTaskGetState                   0

/* Cortex-M NVIC 인터럽트 우선순위 ----------------------------------------- */
/* STM32F103은 우선순위 비트 4개를 사용합니다. CMSIS 정의가 있으면 우선합니다. */
#ifdef __NVIC_PRIO_BITS
#define configPRIO_BITS                         __NVIC_PRIO_BITS
#else
#define configPRIO_BITS                         4
#endif

/*
 * Cortex-M에서는 숫자가 작을수록 인터럽트 우선순위가 높습니다.
 * 우선순위 0~4 ISR에서는 FreeRTOS API를 호출하면 안 됩니다.
 * 우선순위 5~15 ISR에서는 이름이 FromISR로 끝나는 API만 호출할 수 있습니다.
 */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY          15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY      5
/* 위 라이브러리 형식 값을 NVIC 레지스터 형식으로 변환한 커널용 값입니다. */
#define configKERNEL_INTERRUPT_PRIORITY \
  (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
  (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/*
 * FreeRTOS 내부 조건 검증에 실패하면 인터럽트를 끄고 해당 위치에서 정지합니다.
 * 디버거로 멈춘 위치와 호출 스택을 확인해 잘못된 API 사용을 찾을 수 있습니다.
 */
#define configASSERT(condition)                    \
  do                                               \
  {                                                \
    if ((condition) == 0)                          \
    {                                              \
      taskDISABLE_INTERRUPTS();                    \
      for (;;)                                     \
      {                                            \
      }                                            \
    }                                              \
  } while (0)

/*
 * FreeRTOS Cortex-M 포트의 예외 처리 함수를 STM32 벡터 테이블의 이름에
 * 직접 연결합니다. SVC는 스케줄러 시작, PendSV는 문맥 전환에 사용됩니다.
 * SysTick_Handler는 stm32f1xx_it.c에서 HAL tick과 함께 별도로 처리합니다.
 */
#define vPortSVCHandler                    SVC_Handler
#define xPortPendSVHandler                 PendSV_Handler

#endif /* FREERTOS_CONFIG_H */
