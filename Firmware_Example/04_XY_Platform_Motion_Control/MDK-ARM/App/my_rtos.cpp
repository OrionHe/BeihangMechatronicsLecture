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
  if (htim->Instance == g_xyPlatform.x->stepper.p_htim->Instance)
  {
    g_xyPlatform.x->ControlLoop();
    // g_xyPlatform.ControlLoop();
  }
  else if (htim->Instance == g_xyPlatform.y->stepper.p_htim->Instance)
  {
    g_xyPlatform.y->ControlLoop();
    // g_xyPlatform.ControlLoop();
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
  g_xyPlatform.MotionConfig(1, 1, 10.0f, 500.0f);
  g_xyPlatform.x->SetMode(x_linear_module::MODULE_MODE_VELOCIY);
  g_xyPlatform.y->SetMode(x_linear_module::MODULE_MODE_VELOCIY);
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
      g_linearModule[0].SetTargetVelocityHard(10.0f);
    }
    if (g_key[1].released())
    {
      g_linearModule[1].SetTargetVelocityHard(10.0f);
    }
    if (g_key[2].released())
    {
      g_linearModule[0].SetTargetVelocityHard(-10.0f);
    }
    if (g_key[3].released())
    {
      g_linearModule[1].SetTargetVelocityHard(-10.0f);
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


