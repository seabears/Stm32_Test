/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
#include "main.h"

/** 최소 시작점: 리셋 기본 클럭인 내부 HSI 8 MHz를 사용한다. */
int main(void)
{
    /* HAL 시간 기준: 1 ms. 외부 크리스털이나 RTOS에 의존하지 않는다. */
    HAL_Init();
    while (1)
    {
        /* 새 프로젝트의 초기화/처리 코드를 이곳에 추가한다. */
        __WFI();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}
