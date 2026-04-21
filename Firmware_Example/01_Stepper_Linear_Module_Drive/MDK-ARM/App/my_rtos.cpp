/**
  ******************************************************************************
  * @file           :
  * @author         : Xiang Guo
  * @brief          : 
  * @date	          : 2023/05/07
  ******************************************************************************
  * @attention
  *
  *
  ******************************************************************************
  */

/* ------------------------------ Includes ------------------------------ */

#include "my_rtos.h"
#include "my_config.h"
#include "main.h"
#include <cstdint>
/* ------------------------------ Defines ------------------------------ */

/* ------------------------------ Variables ------------------------------ */

/* ------------------------------ Functions ------------------------------ */

#ifdef __cplusplus
extern "C" {
#endif

void My_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

void StartDefaultTask(void *argument);
void StartDebugTask(void *argument);
void StartKeyScanTask(void *argument);
#ifdef __cplusplus
}
#endif

/* ------------------------------ Interrupts ------------------------------ */

void My_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{

  if (htim->Instance == g_stepper[0].p_htim->Instance)
  {
    g_stepper[0].ControlLoop();
  }
  else if (htim->Instance == g_stepper[1].p_htim->Instance)
  {
    g_stepper[1].ControlLoop();
  }
}

/* ------------------------------ Tasks ------------------------------ */

/**
  * @brief  .
  * @author Xiang Guo
  * @param  none
  * @retval none
  */
void StartDefaultTask(void *argument)
{

  uint32_t pulse_per_sec = 16000;
  uint32_t pulse_acc = 160000;
  g_stepper[0].MotionConfig(1, pulse_per_sec, pulse_acc);
  g_stepper[1].MotionConfig(1, pulse_per_sec, pulse_acc);
  g_stepper[0].SetMode(xstepper::STEPPER_MODE_VELOCITY);
  g_stepper[1].SetMode(xstepper::STEPPER_MODE_VELOCITY);
  osThreadResume(debugTaskHandle);
  osThreadResume(keyScanTaskHandle);
  osThreadSuspend(defaultTaskHandle);
  /* Infinite loop */
  for (;;)
  {}
}

/**
  * @brief  .
  * @param  none
  * @retval none
  */
void StartDebugTask(void *argument)
{
  for (;;)
  { 
     if (g_key[0].released())
     {
      // HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
      g_stepper[0].SetTargetVelocity(900.0f);
     }
     if (g_key[1].released())
     {
      // HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
      g_stepper[0].SetTargetVelocity(-900.0f);
     }
     if (g_key[2].released())
     {
      // HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
     }
     if (g_key[3].released()) // stop
     {
      // HAL_GPIO_TogglePin(LED4_GPIO_Port, LED4_Pin);
      g_stepper[0].SetVelocityHard(0.0f);
     }
     osDelay(10);
  }
}

/**
  * @brief  按键扫描任务，50ms扫描一次按键，更新按键状态，按键按下时，更新按键状态为按下，按键释放时，更新按键状态为释放
  * @param  none
  * @retval none
  */
void StartKeyScanTask(void *argument)
{
  for (;;)
  {
    for (uint8_t i = 0; i < 4; i++)
    {
      g_key[i].update();
    }
    osDelay(50);
  }
}

