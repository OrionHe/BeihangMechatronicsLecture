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

#include "XYplatform.h"
#include "arm_math.h"
#include "xLinearModule.h"

/* ------------------------------ Defines ------------------------------ */

#define abs(x) ((x) > 0 ? (x) : -(x))
#define POSITION_ERROR_THRESHOLD 1e-2f

/* ------------------------------ variables ------------------------------ */

/* ------------------------------ Functions ------------------------------ */

static float LinearInterJudge(float x_real, float y_real, float x_target,
                              float y_target, float x_start, float y_start) {
  float x_e = x_target - x_start;
  float y_e = y_target - y_start;
  float x_i = x_real - x_start;
  float y_i = y_real - y_start;
  float f = x_e * y_i - x_i * y_e;
  return f;
}

namespace xy_platform {
XYplatform::XYplatform(x_linear_module::LinearModule *x,
                       x_linear_module::LinearModule *y, float max_vel,
                       float pid_kp, float pid_ki, float pid_kd,
                       float pid_time_period_s)
    : pos_pid_x(pid_kp, pid_ki, pid_kd, max_vel, pid_time_period_s),
      pos_pid_y(pid_kp, pid_ki, pid_kd, max_vel, pid_time_period_s) {
  this->x = x;
  this->y = y;
}

void XYplatform::MotionConfig(int8_t x_dir, int8_t y_dir, float max_vel,
                              float acc) {
  x->MotionConfig(x_dir, max_vel, acc);
  y->MotionConfig(y_dir, max_vel, acc);
  this->max_vel = max_vel;
}

void XYplatform::FindHome(void) {
  this->mode = PLATFORM_MODE_FIND_HOME;
  x->SetMode(x_linear_module::MODULE_MODE_VELOCIY);
  y->SetMode(x_linear_module::MODULE_MODE_VELOCIY);
  x->SetTargetVelocity(-10.0f);
  y->SetTargetVelocity(-10.0f);
}

void XYplatform::MoveTo(float x, float y) {
  // 请完成此函数
  // 设置模式为手动模式
  this->mode = PLATFORM_MODE_MANUAL;
  // 计算x和y的速度
  float vel_x =
      abs(x - this->x_real) > abs(y - this->y_real)
          ? this->max_vel
          : this->max_vel * (x - this->x_real) / abs(y - this->y_real);
  float vel_y = abs(x - this->x_real) > abs(y - this->y_real)
                    ? this->max_vel * (y - this->y_real) / abs(x - this->x_real)
                    : this->max_vel;
  vel_x = x - this->x_real > 0 ? vel_x : -vel_x;
  vel_y = y - this->y_real > 0 ? vel_y : -vel_y;
  // 设置x和y目标位置
  this->x->SetMode(x_linear_module::MODULE_MODE_POSITION);
  this->y->SetMode(x_linear_module::MODULE_MODE_POSITION);
  this->x->SetTargetPositionWithVelocity(x, vel_x);
  this->y->SetTargetPositionWithVelocity(y, vel_y);
}

void XYplatform::LinearInterpolation(float x, float y, float vel, float step) {
  // 请完成此函数
  // 设置模式为线性插补模式
  this->mode = PLATFORM_MODE_LINEAR_INTERPOLATION;
  // 记录插补起始位置
  this->x_interpolation_start = this->x_real;
  this->y_interpolation_start = this->y_real;
  this->x_interpolation_target = this->x_real;
  this->y_interpolation_target = this->y_real;
  // 设置插补目标位置
  this->x_target = x;
  this->y_target = y;
  // 设置插补速度
  this->inter_vel = vel;
  // 设置插补步长
  this->inter_step = step;

  this->x->SetMode(x_linear_module::MODULE_MODE_POSITION);
  this->y->SetMode(x_linear_module::MODULE_MODE_POSITION);
}

void XYplatform::CircularInterpolation(float center_x, float center_y,
                                       float radius, float vel, float angle,
                                       bool clockwise, float step) {
  // 请完成此函数
  // 设置模式为圆弧插补模式
  this->mode = PLATFORM_MODE_CIRCULAR_INTERPOLATION;
  // 记录插补起始位置
  this->x_interpolation_start = this->x_real;
  this->y_interpolation_start = this->y_real;
  this->x_interpolation_target = this->x_real;
  this->y_interpolation_target = this->y_real;
  // 设置插补目标位置
  this->x_target = center_x + radius * arm_cos_f32(angle);
  this->y_target = center_y + radius * arm_sin_f32(angle);
  // 设置插补速度
  this->inter_vel = vel;
  // 设置插补步长
  this->inter_vel = step;
  // 设置圆弧插补方向
  this->clockwise = clockwise;
  // 设置圆弧插补半径
  this->radius = radius;

  this->x->SetMode(x_linear_module::MODULE_MODE_POSITION);
  this->y->SetMode(x_linear_module::MODULE_MODE_POSITION);
}

void XYplatform::ClosedLoopControl(float x_pos_ref, float y_pos_ref) {
  // 设置模式为闭环位置控制模式
  this->mode = PLATFORM_MODE_CLOSED_LOOP;
  // 设置x和y目标位置
  this->x_target = x_pos_ref;
  this->y_target = y_pos_ref;

  this->x->SetMode(x_linear_module::MODULE_MODE_VELOCIY);
  this->y->SetMode(x_linear_module::MODULE_MODE_VELOCIY);
}

void XYplatform::ControlLoop(void) {
  // 请完成此函数 Start

  // 更新x和y的实际位置
  this->x_real = this->x->GetPosition();
  this->y_real = this->y->GetPosition();

  // 根据模式进行不同的控制
  if (this->mode == PLATFORM_MODE_IDLE) {
  } else if (this->mode == PLATFORM_MODE_MANUAL) {
  } else if (this->mode == PLATFORM_MODE_FIND_HOME) {
    if (abs(this->x_real) <= POSITION_ERROR_THRESHOLD &&
        abs(this->y_real) <= POSITION_ERROR_THRESHOLD) {
      this->mode = PLATFORM_MODE_MANUAL;
    }
  } else if (this->mode == PLATFORM_MODE_LINEAR_INTERPOLATION) {
    // 检查是否到达目标位置
    if (abs(this->x_real - this->x_target) <= POSITION_ERROR_THRESHOLD &&
        abs(this->y_real - this->y_target) <= POSITION_ERROR_THRESHOLD) {
      this->mode = PLATFORM_MODE_MANUAL;
    }
    // 计算插补目标位置
    else if (this->x_target - this->x_real >= 0 &&
             this->y_target - this->y_real >= 0) {
      // 第一象限
      if (LinearInterJudge(this->x_real, this->y_real, this->x_target,
                           this->y_target, this->x_interpolation_start,
                           this->y_interpolation_start) > 0) {
        // 是否可以一步完成
        if (abs(this->x_target - this->x_interpolation_target) <=
            this->inter_step) {
          this->x_interpolation_target = this->x_target;
        } else {
          this->x_interpolation_target = this->x_real + this->inter_step;
        }
      } else {
        // 是否可以一步完成
        if (abs(this->y_target - this->y_interpolation_target) <=
            this->inter_step) {
          this->y_interpolation_target = this->y_target;
        } else {
          this->y_interpolation_target = this->y_real + this->inter_step;
        }
      }
    } else if (this->x_target - this->x_real <= 0 &&
               this->y_target - this->y_real >= 0) {
      // 第二象限
      if (LinearInterJudge(this->x_real, this->y_real, this->x_target,
                           this->y_target, this->x_interpolation_start,
                           this->y_interpolation_start) > 0) {
        // 是否可以一步完成
        if (abs(this->x_target - this->x_interpolation_target) <=
            this->inter_step) {
          this->x_interpolation_target = this->x_target;
        } else {
          this->x_interpolation_target = this->x_real - this->inter_step;
        }
      } else {
        // 是否可以一步完成
        if (abs(this->y_target - this->y_interpolation_target) <=
            this->inter_step) {
          this->y_interpolation_target = this->y_target;
        } else {
          this->y_interpolation_target = this->y_real + this->inter_step;
        }
      }
    } else if (this->x_target - this->x_real <= 0 &&
               this->y_target - this->y_real <= 0) {
      // 第三象限
      if (LinearInterJudge(this->x_real, this->y_real, this->x_target,
                           this->y_target, this->x_interpolation_start,
                           this->y_interpolation_start) > 0) {
        // 是否可以一步完成
        if (abs(this->x_target - this->x_interpolation_target) <=
            this->inter_step) {
          this->x_interpolation_target = this->x_target;
        } else {
          this->x_interpolation_target = this->x_real - this->inter_step;
        }
      } else {
        // 是否可以一步完成
        if (abs(this->y_target - this->y_interpolation_target) <=
            this->inter_step) {
          this->y_interpolation_target = this->y_target;
        } else {
          this->y_interpolation_target = this->y_real - this->inter_step;
        }
      }
    } else if (this->x_target - this->x_real >= 0 &&
               this->y_target - this->y_real <= 0) {
      // 第四象限
      if (LinearInterJudge(this->x_real, this->y_real, this->x_target,
                           this->y_target, this->x_interpolation_start,
                           this->y_interpolation_start) > 0) {
        // 是否可以一步完成
        if (abs(this->x_target - this->x_interpolation_target) <=
            this->inter_step) {
          this->x_interpolation_target = this->x_target;
        } else {
          this->x_interpolation_target = this->x_real + this->inter_step;
        }
      } else {
        // 是否可以一步完成
        if (abs(this->y_target - this->y_interpolation_target) <=
            this->inter_step) {
          this->y_interpolation_target = this->y_target;
        } else {
          this->y_interpolation_target = this->y_real - this->inter_step;
        }
      }
    }
    // 设定插补目标位置
    this->x->SetTargetPositionWithVelocity(this->x_interpolation_target,
                                           this->inter_vel);
    this->y->SetTargetPositionWithVelocity(this->y_interpolation_target,
                                           this->inter_vel);
  } else if (this->mode == PLATFORM_MODE_CIRCULAR_INTERPOLATION) {
  } else if (this->mode == PLATFORM_MODE_CLOSED_LOOP) {
    // 判断是否到达目标位置
    if (abs(this->x_real - this->x_target) <= POSITION_ERROR_THRESHOLD &&
        abs(this->y_real - this->y_target) <= POSITION_ERROR_THRESHOLD) {
      this->x->SetTargetVelocity(0);
      this->y->SetTargetVelocity(0);
      this->mode = PLATFORM_MODE_IDLE;
      // 清零PID积分项
      this->pos_pid_x.integral = 0;
      this->pos_pid_y.integral = 0;
      return;
    }
    // 计算x和y的目标速度
    float vel_x = pos_pid_x.Calc(this->x_real);
    float vel_y = pos_pid_y.Calc(this->y_real);
    // 设置x和y的目标速度
    this->x->SetTargetVelocity(vel_x);
    this->y->SetTargetVelocity(vel_y);
  }

  // 请完成此函数 End
}
} // namespace xy_platform
