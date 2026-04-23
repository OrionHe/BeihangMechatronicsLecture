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
  if (htim->Instance == g_linearModule[0].stepper.p_htim->Instance)
  {
    g_linearModule[0].ControlLoop();
  }
  else if (htim->Instance == g_linearModule[1].stepper.p_htim->Instance)
  {
    g_linearModule[1].ControlLoop();
  }
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  osDelay(20);

  if (GPIO_Pin ==  g_linearModule[1].limit_switch1_pin)
  {
    if (HAL_GPIO_ReadPin(g_linearModule[1].limit_switch1_port, g_linearModule[1].limit_switch1_pin) == GPIO_PIN_SET)
    {
      //y轴回零边界
      if (g_linearModule[1].mode != x_linear_module::MODULE_MODE_POSITION)
      {
        g_linearModule[1].SetMode(x_linear_module::MODULE_MODE_POSITION);
        g_linearModule[1].SetPosition(-10);
        g_linearModule[1].SetTargetPosition(0);
        g_linearModule[1].SetTargetVelocityHard(0);
      }
    }
  }
  else if (GPIO_Pin == g_linearModule[1].limit_switch2_pin)
  {
    //y轴上限边界，对应错误状态
    if (HAL_GPIO_ReadPin(g_linearModule[1].limit_switch2_port, g_linearModule[1].limit_switch2_pin  ) == GPIO_PIN_SET)
    {
        // Handle limit switch trigger
        g_linearModule[1].SetMode(x_linear_module::MODULE_MODE_ERROR);
        g_linearModule[1].SetTargetVelocityHard(0);
    }
  }
  else if (GPIO_Pin==  g_linearModule[0].limit_switch1_pin)
  {
    //x轴回零边界
    if (HAL_GPIO_ReadPin( g_linearModule[0].limit_switch1_port,  g_linearModule[0].limit_switch1_pin) == GPIO_PIN_SET)
    {
      if (g_linearModule[0].mode != x_linear_module::MODULE_MODE_POSITION)
      {
           g_linearModule[0].SetMode(x_linear_module::MODULE_MODE_POSITION);
           g_linearModule[0].SetPosition(-10);
           g_linearModule[0].SetTargetPosition(0);
           g_linearModule[0].SetTargetVelocityHard(0);
      }
    }
  }
	else if (GPIO_Pin ==  g_linearModule[0].limit_switch2_pin)
  {
    //x轴上限边界，对应错误状态
    if (HAL_GPIO_ReadPin( g_linearModule[0].limit_switch2_port,  g_linearModule[0].limit_switch2_pin) == GPIO_PIN_SET)
    {
       g_linearModule[0].SetMode(x_linear_module::MODULE_MODE_ERROR);
       g_linearModule[0].SetTargetVelocityHard(0);
    }
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
  g_linearModule[0].MotionConfig(1, 10.0f, 500.0f);
  g_linearModule[1].MotionConfig(1, 10.0f, 500.0f);
  g_linearModule[0].SetMode(x_linear_module::MODULE_MODE_VELOCITY);
  g_linearModule[1].SetMode(x_linear_module::MODULE_MODE_VELOCITY);
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
    if (g_key[0].released()) // find home position
    {
			  g_linearModule[0].SetMode(x_linear_module::MODULE_MODE_VELOCITY);
  g_linearModule[1].SetMode(x_linear_module::MODULE_MODE_VELOCITY);
      g_linearModule[0].SetTargetVelocity(-10.0f);
      g_linearModule[1].SetTargetVelocity(-10.0f);
    }
    if (g_key[1].released())
    {
			  g_linearModule[0].SetMode(x_linear_module::MODULE_MODE_VELOCITY);
  g_linearModule[1].SetMode(x_linear_module::MODULE_MODE_VELOCITY);
      g_linearModule[0].SetTargetVelocityHard(0.0f);
      g_linearModule[1].SetTargetVelocityHard(0.0f);
    }
    if (g_key[2].released())
    {
			  g_linearModule[0].SetMode(x_linear_module::MODULE_MODE_VELOCITY);
  g_linearModule[1].SetMode(x_linear_module::MODULE_MODE_VELOCITY);
      g_linearModule[0].SetTargetVelocity(10.0f);
      g_linearModule[1].SetTargetVelocity(10.0f);
    }
    if (g_key[3].released())
    {

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
