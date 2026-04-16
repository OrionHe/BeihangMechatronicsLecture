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
#ifndef __XY_PLATFORM_H
#define __XY_PLATFORM_H

/* ------------------------------ Includes ------------------------------ */

#include "xLinearModule.h"
#include "pid.h"

/* ------------------------------ Defines ------------------------------ */

/* ------------------------------ Class ------------------------------ */

using pid::Pid;

namespace xy_platform {
typedef enum {
  PLATFORM_MODE_IDLE,
  PLATFORM_MODE_MANUAL,
  PLATFORM_MODE_FIND_HOME,
  PLATFORM_MODE_LINEAR_INTERPOLATION,
  PLATFORM_MODE_CIRCULAR_INTERPOLATION,
  PLATFORM_MODE_CLOSED_LOOP,
} PlatformMode_t;
class XYplatform {
public:
  x_linear_module::LinearModule *x, *y;
  float max_vel; // 每个直线模组的最大速度，单位mm/s

  // 请完成此类的成员变量 Start

  /* XY平台执行状态 */
  PlatformMode_t mode = PLATFORM_MODE_IDLE;
  float x_target, y_target; // 最终目标位置，单位mm
  float x_interpolation_start,
      y_interpolation_start; // 当前插补步起始位置，单位mm
  float x_interpolation_target,
      y_interpolation_target; // 当前插补步目标位置，单位mm
  float x_real, y_real;       // 当前实际位置，单位mm
  float x_vel, y_vel;         // 当前速度，单位mm/s
  float inter_vel;            // 当前插补速度，单位mm/s
  float inter_step = 0.1f;    // 当前插补步长，单位mm
  /* 圆弧插补参数 */
  float x_center, y_center; // 当前圆弧插补圆心，单位mm
  float radius;             // 当前圆弧插补半径，单位mm
  bool clockwise;           // 当前圆弧插补方向

  // pid 控制器
  Pid pos_pid_x;
  Pid pos_pid_y;

  // 请完成此类的成员变量 End

  XYplatform(x_linear_module::LinearModule *x,
             x_linear_module::LinearModule *y,
             float max_vel,
             float pid_kp = 2.0f,
             float pid_ki = 0.0f,
             float pid_kd = 0.0f,
             float pid_time_period_s = 0.01f);

  /**
   * @brief  设置XY平台运动参数
   * @author Xiang Guo
   * @param  x_dir: X轴方向，1为正向，-1为反向
   * @param  y_dir: Y轴方向，1为正向，-1为反向
   * @param  max_vel: 每轴最大速度，单位mm/s
   * @param  acc: 每轴加速度，单位mm/s^2
   * @retval none
   */
  void MotionConfig(int8_t x_dir, int8_t y_dir, float max_vel, float acc);

  /**
   * @brief
   * 回零点，回零点速度为-10mm/s，回零点后速度为0mm/s，回零点后位置为0mm，回零点后模式为PLATFORM_MODE_MANUAL，回零点后清除所有目标速度和位置
   * @author Xiang Guo
   * @param  none
   * @retval none
   */
  void FindHome(void);

  /**
   * @brief
   * 直线运动到目标位置，目标速度为最大速度，运动到目标位置后速度为0mm/s，路径不是精确直线
   * @author Xiang Guo
   * @param  x: X轴目标位置，单位mm
   * @param  y: Y轴目标位置，单位mm
   * @retval none
   */
  void MoveTo(float x, float y);

  /**
   * @brief
   * 通过直线插补运动到目标位置，运动到目标位置后速度为0mm/s，模式为PLATFORM_MODE_MANUAL，路径是精确直线（误差为插值步长）
   * @author Xiang Guo
   * @param  x: X轴目标位置，单位mm
   * @param  y: Y轴目标位置，单位mm
   * @param  vel: 插补速度，单位mm/s
   * @param  step: 插补步长，单位mm
   * @retval none
   */
  void LinearInterpolation(float x, float y, float vel, float step);

  /**
   * @brief
   * 通过圆弧插补运动到目标位置，运动到目标位置后速度为0mm/s，模式为PLATFORM_MODE_MANUAL，路径是精确圆弧（误差为插值步长）
   * @author Xiang Guo
   * @param  center_x: 圆心X轴坐标，单位mm
   * @param  center_y: 圆心Y轴坐标，单位mm
   * @param  radius: 圆弧半径，单位mm
   * @param  vel: 插补速度，单位mm/s
   * @param  angle: 圆弧角度，单位°
   * @param  clockwise: 圆弧方向，true为顺时针，false为逆时针
   * @param  step: 插补步长，单位mm
   * @retval none
   */
  void CircularInterpolation(float center_x, float center_y, float radius,
                             float vel, float angle, bool clockwise,
                             float step);

  /**
   * @brief  闭环位置控制，将平台移动到参考位置
   * @author Xiang Guo
   * @param  x_pos_ref: X轴参考位置，单位mm
   * @param  y_pos_ref: Y轴参考位置，单位mm
   * @retval none
   */
  void ClosedLoopControl(float x_pos_ref, float y_pos_ref);

  /**
   * @brief  XY平台控制回路，用来实现插补控制，在每轴PWM定时器中断中都需要调用
   * @author Xiang Guo
   * @param  none
   * @retval none
   */
  void ControlLoop(void);
};

} // namespace xy_platform

#endif
