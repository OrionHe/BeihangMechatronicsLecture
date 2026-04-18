/**
  ******************************************************************************
  * @file           : 
  * @author         : Xiang Guo
  * @date           : 2023/xx/xx
  * @brief          : 
  ******************************************************************************
  * @attention
  *             
  *             
  ******************************************************************************
  */

/* ------------------------------ Includes ------------------------------ */

#include "xLinearModule.h"
#include "main.h"

/* ------------------------------ Defines ------------------------------ */

/* ------------------------------ variables ------------------------------ */

/* ------------------------------ Functions ------------------------------ */

namespace x_linear_module
{
  LinearModule::LinearModule(TIM_HandleTypeDef *p_htim, uint32_t channel, uint32_t tim_freq, float step_angle, float step_division,
                             GPIO_TypeDef *dir_port, uint16_t dir_pin, GPIO_TypeDef *n_enable_port, uint16_t n_enable_pin,
                             GPIO_TypeDef *limit_switch1_port, uint16_t limit_switch1_pin,
                             GPIO_TypeDef *limit_switch2_port, uint16_t limit_switch2_pin,
                             float lead)
      : stepper(p_htim, channel, tim_freq, step_angle, step_division, dir_port, dir_pin, n_enable_port, n_enable_pin),
        limit_switch1_port(limit_switch1_port), limit_switch1_pin(limit_switch1_pin),
        limit_switch2_port(limit_switch2_port), limit_switch2_pin(limit_switch2_pin),
        lead(lead)
  {
  }

  void LinearModule::MotionConfig(int8_t dir, float max_vel, float acc)
  {
    uint32_t step_max_vel = (uint32_t) (max_vel / lead * 360.0f * stepper.step_division / stepper.step_angle);
    uint32_t step_acc = (uint32_t) (acc / lead * 360.0f * stepper.step_division / stepper.step_angle);
    this->stepper.MotionConfig(dir, step_max_vel, step_acc);
  }

  void LinearModule::SetMode(ModuleMode_t mode)
  {
    this->mode = mode;
    switch (mode)
    {
      case MODULE_MODE_IDLE:
        this->stepper.SetMode(xstepper::STEPPER_MODE_IDLE);
        break;
      case MODULE_MODE_VELOCIY:
        this->stepper.SetMode(xstepper::STEPPER_MODE_VELOCITY);
        break;
      case MODULE_MODE_POSITION:
        this->stepper.SetMode(xstepper::STEPPER_MODE_POSITION);
        break;
      default:
        break;
    }
  }

  void LinearModule::SetPosition(float position)
  {
    this->stepper.SetAngle(position / lead * 360.0f);
  }

  void LinearModule::SetTargetPosition(float position)
  {
    this->stepper.SetTargetAngle(position / lead * 360.0f);
  }

  void LinearModule::SetTargetPositionWithVelocity(float position, float velocity)
  {
    this->stepper.SetTargetAngleWithVel(position / lead * 360.0f, velocity / lead * 360.0f);
  }

  void LinearModule::SetTargetVelocity(float velocity)
  {
    this->stepper.SetTargetVelocity(velocity / lead * 360.0f);
  }

  void LinearModule::SetTargetVelocityHard(float velocity)
  {
    this->stepper.SetVelocityHard(velocity / lead * 360.0f);
  }

  float LinearModule::GetPosition(void)
  {
    return (float)((float)(this->stepper.step_current_angle) * (float)(this->stepper.step_angle) * this->lead / this->stepper.step_division / 360.0f);
  }

  void LinearModule::ControlLoop(void)
  {
    // 限位检测
    if (HAL_GPIO_ReadPin(this->limit_switch1_port, this->limit_switch1_pin) == GPIO_PIN_SET)
    {
      HAL_GPIO_TogglePin(LED1_GPIO_Port,LED1_Pin);
      if (this->mode != MODULE_MODE_POSITION)
      {
        this->SetMode(MODULE_MODE_POSITION);
        this->SetPosition(-10);
        this->SetTargetPosition(0);
        this->SetTargetVelocityHard(0);
      }
    }
    else if (HAL_GPIO_ReadPin(this->limit_switch2_port, this->limit_switch2_pin) == GPIO_PIN_SET)
    {
      HAL_GPIO_TogglePin(LED2_GPIO_Port,LED2_Pin);
      this->SetMode(MODULE_MODE_ERROR);
      this->SetTargetVelocityHard(0);
    }
    this->stepper.ControlLoop();
  }
  
}

