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
#include "usbd_cdc_if.h"
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
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  osDelay(20);
  if (GPIO_Pin==g_xyPlatform.x->limit_switch1_pin)
    {
      if (HAL_GPIO_ReadPin(g_xyPlatform.x->limit_switch1_port, g_xyPlatform.x->limit_switch1_pin) == GPIO_PIN_RESET)
      {
        if (g_xyPlatform.x->mode != x_linear_module::MODULE_MODE_POSITION)
        {
          // Handle limit switch trigger
          g_xyPlatform.x->SetMode(x_linear_module::MODULE_MODE_POSITION);
          g_xyPlatform.x->SetPosition(-10);
          g_xyPlatform.x->SetTargetPosition(0);
          g_xyPlatform.x->SetTargetVelocityHard(0);
        }
      }
    }
    else if (GPIO_Pin == g_xyPlatform.x->limit_switch2_pin)
    {
      if (HAL_GPIO_ReadPin(g_xyPlatform.x->limit_switch2_port, g_xyPlatform.x->limit_switch2_pin) == GPIO_PIN_RESET)
        {
          // Handle limit switch trigger
          g_xyPlatform.x->SetMode(x_linear_module::MODULE_MODE_ERROR);
          g_xyPlatform.x->SetTargetVelocityHard(0);
        }
    }
    else if (GPIO_Pin == g_xyPlatform.y->limit_switch1_pin)
    {
      if (HAL_GPIO_ReadPin(g_xyPlatform.y->limit_switch1_port, g_xyPlatform.y->limit_switch1_pin) == GPIO_PIN_RESET)
      {
        if (g_xyPlatform.y->mode != x_linear_module::MODULE_MODE_POSITION)
        {
          // Handle limit switch trigger
          g_xyPlatform.y->SetMode(x_linear_module::MODULE_MODE_POSITION);
          g_xyPlatform.y->SetPosition(-10);
          g_xyPlatform.y->SetTargetPosition(0);
          g_xyPlatform.y->SetTargetVelocityHard(0);
        }
      }
    }
    else if (GPIO_Pin == g_xyPlatform.y->limit_switch2_pin)
    {
      if (HAL_GPIO_ReadPin(g_xyPlatform.y->limit_switch2_port, g_xyPlatform.y->limit_switch2_pin  ) == GPIO_PIN_RESET)
      {
        // Handle limit switch trigger
        g_xyPlatform.y->SetMode(x_linear_module::MODULE_MODE_ERROR);
        g_xyPlatform.y->SetTargetVelocityHard(0);
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
  MX_USB_DEVICE_Init();
  g_xyPlatform.MotionConfig(1, 1, 10.0f, 500.0f);
  g_xyPlatform.x->SetMode(x_linear_module::MODULE_MODE_VELOCITY);
  g_xyPlatform.y->SetMode(x_linear_module::MODULE_MODE_VELOCITY);
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
//     if (g_key[0].released())
//     {
// //      g_linearModule[0].SetTargetVelocity(-10.0f);
//         g_xyPlatform.FindHome();
//     }
//     if (g_key[1].released())
//     {
// //      g_linearModule[1].SetTargetVelocity(-10.0f);
//       g_linearModule[0].SetTargetVelocityHard(0.0f);
//       g_linearModule[1].SetTargetVelocityHard(0.0f);
//     }
//     if (g_key[2].released())
//     {
//       g_linearModule[0].SetTargetVelocity(10.0f);
//     }
//     if (g_key[3].released())
//     {
//       g_linearModule[1].SetTargetVelocity(10.0f);
//     }
    g_xyPlatform.ControlLoop();
    osDelay(1);
  }
}

/**
  * @brief  Key scan task, update key states every 50ms.
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
  (void)argument;

  uint8_t cmd = 0U;
  uint8_t *data = nullptr;
  uint8_t data_len = 0U;
  uint8_t packet_buf[64];
  uint32_t packet_len = 0U;
  uint8_t stream_buf[512];
  uint16_t stream_len = 0U;

  for (;;)
  {
    (void)osThreadFlagsWait(USB_RX_THREAD_FLAG_DATA, osFlagsWaitAny, osWaitForever);

    while (USB_CDC_RxPop(packet_buf, sizeof(packet_buf), &packet_len))
    {
      if ((stream_len + packet_len) > sizeof(stream_buf))
      {
        stream_len = 0U;
      }

      memcpy(&stream_buf[stream_len], packet_buf, packet_len);
      stream_len += (uint16_t)packet_len;

      uint16_t parse_pos = 0U;
      while ((stream_len - parse_pos) >= 5U)
      {
        if (stream_buf[parse_pos] != FRAME_HEADER)
        {
          parse_pos++;
          continue;
        }

        uint16_t frame_len = (uint16_t)stream_buf[parse_pos + 2U] + 5U;
        if ((stream_len - parse_pos) < frame_len)
        {
          break;
        }

        if (stream_buf[parse_pos + frame_len - 1U] == FRAME_TAIL)
        {
          if (usb_parse_command(&stream_buf[parse_pos], frame_len, &cmd, &data, &data_len))
          {
            usb_handle_command(cmd, data, data_len);
          }
          parse_pos += frame_len;
        }
        else
        {
          parse_pos++;
        }
      }

      if (parse_pos > 0U)
      {
        uint16_t remain = (uint16_t)(stream_len - parse_pos);
        if (remain > 0U)
        {
          memmove(stream_buf, &stream_buf[parse_pos], remain);
        }
        stream_len = remain;
      }
    }
  }
}
