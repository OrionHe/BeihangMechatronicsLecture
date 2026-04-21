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

#include "my_config.h"
#include "main.h"
#include "tim.h"
#include "xkey.h"


/* ------------------------------ Defines ------------------------------ */

/* ------------------------------ Variables ------------------------------ */

xkey::Key g_key[4] = {xkey::Key(KEY1_GPIO_Port, KEY1_Pin, GPIO_PIN_RESET),
                      xkey::Key(KEY2_GPIO_Port, KEY2_Pin, GPIO_PIN_RESET),
                      xkey::Key(KEY3_GPIO_Port, KEY3_Pin, GPIO_PIN_RESET),
                      xkey::Key(KEY4_GPIO_Port, KEY4_Pin, GPIO_PIN_RESET)};

xstepper::Stepper g_stepper[2] = {
    xstepper::Stepper(&htim8, TIM_CHANNEL_4, STIM_FREQ, 1.8f, 32,
    DIR_M1_GPIO_Port, DIR_M1_Pin, nENBL_M1_GPIO_Port, nENBL_M1_Pin),
    xstepper::Stepper(&htim3, TIM_CHANNEL_1, STIM_FREQ, 1.8f, 32,
    DIR_M2_GPIO_Port, DIR_M2_Pin, nENBL_M2_GPIO_Port, nENBL_M2_Pin),
    };

/* ------------------------------ Functions ------------------------------ */
