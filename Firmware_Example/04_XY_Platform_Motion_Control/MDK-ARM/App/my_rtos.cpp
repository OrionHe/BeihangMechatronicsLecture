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
#include "xusb.h"
#include "usb_device.h"
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
void StartUsbRxTask(void *argument);
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
  MX_USB_DEVICE_Init();
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
//      g_linearModule[0].SetTargetVelocity(-10.0f);
        g_xyPlatform.FindHome();
    }
    if (g_key[1].released())
    {
//      g_linearModule[1].SetTargetVelocity(-10.0f);
      g_linearModule[0].SetTargetVelocityHard(0.0f);
      g_linearModule[1].SetTargetVelocityHard(0.0f);
    }
    if (g_key[2].released())
    {
      g_linearModule[0].SetTargetVelocity(10.0f);
    }
    if (g_key[3].released())
    {
      g_linearModule[1].SetTargetVelocity(10.0f);
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

void StartUsbRxTask(void *argument)
{
  uint8_t cmd;
  uint8_t *data;
  uint8_t data_len;
  for (;;)
  {
    if (flag_usb)
    {
      // Process USB received data
      if(usb_parse_command(Buffer_usb, Len_usb, &cmd, &data, &data_len))
      {
        usb_handle_command(cmd, data, data_len);
      }
      flag_usb = 0;
    }
    osDelay(10);
  }
}
