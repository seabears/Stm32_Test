#include "Module_FreeRtos.h"

#include "FreeRTOS.h"
#include "task.h"

#include "Module_Debug.h"
#include "Module_Usb.h"
#include "main.h"

#define TASK_STACK_WORDS 128U

static void Task1ms(void *argument);
static void Task10ms(void *argument);
static void Task100ms(void *argument);
static void Task1000ms(void *argument);

void ModuleFreeRtos_Start(void)
{
  BaseType_t result;

  result = xTaskCreate(Task1ms, "task1ms", TASK_STACK_WORDS, NULL,
                       tskIDLE_PRIORITY + 4U, NULL);
  if (result != pdPASS)
  {
    Error_Handler();
  }

  result = xTaskCreate(Task10ms, "task10ms", TASK_STACK_WORDS, NULL,
                       tskIDLE_PRIORITY + 3U, NULL);
  if (result != pdPASS)
  {
    Error_Handler();
  }

  result = xTaskCreate(Task100ms, "task100ms", TASK_STACK_WORDS, NULL,
                       tskIDLE_PRIORITY + 2U, NULL);
  if (result != pdPASS)
  {
    Error_Handler();
  }

  result = xTaskCreate(Task1000ms, "task1000ms", TASK_STACK_WORDS, NULL,
                       tskIDLE_PRIORITY + 1U, NULL);
  if (result != pdPASS)
  {
    Error_Handler();
  }

  vTaskStartScheduler();
  Error_Handler();
}

static void Task1ms(void *argument)
{
  TickType_t lastWakeTime = xTaskGetTickCount();
  static unsigned int i = 0;

  (void)argument;

  for (;;)
  {
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1U));
    /* Add 1 ms work here. */

    if (i < 1U)
    {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
    }
    else
    {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
    }

    i++;
    if (i >= 10U)
    {
      i = 0U;
    }
  }
}

static void Task10ms(void *argument)
{
  TickType_t lastWakeTime = xTaskGetTickCount();
  (void)argument;

  for (;;)
  {
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(10U));
    /* Add 10 ms work here. */
  }
}

static void Task100ms(void *argument)
{
  TickType_t lastWakeTime = xTaskGetTickCount();
  (void)argument;

  for (;;)
  {
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(100U));
    /* Add 100 ms work here. */
  }
}

static void Task1000ms(void *argument)
{
  TickType_t lastWakeTime = xTaskGetTickCount();
  static uint8 testPacketSent = 0U;
  static uint8 testDelaySeconds = 0U;
  (void)argument;

  for (;;)
  {
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000U));

    if (testDelaySeconds < 10U)
    {
      testDelaySeconds++;
    }

    /* PC가 COM 포트를 열 시간을 준 뒤 테스트 패킷을 정확히 한 번 전송합니다. */
    if ((testPacketSent == 0U) && (testDelaySeconds >= 10U))
    {
      if (ModuleDebug_SendTestResponse() == MODULE_DEBUG_TRANSMIT_OK)
      {
        testPacketSent = 1U;
      }
    }
  }
}

void vApplicationMallocFailedHook(void)
{
  taskDISABLE_INTERRUPTS();
  for (;;)
  {
  }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *taskName)
{
  (void)task;
  (void)taskName;
  taskDISABLE_INTERRUPTS();
  for (;;)
  {
  }
}
