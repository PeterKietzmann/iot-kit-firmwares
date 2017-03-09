/*
 * Copyright (C) 2017 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief       SmartWindow open/close functions
 *
 * @author      Peter Kietzmann <peter.kietzmann@haw-hamburg.de>
 *
 * @}
 */

#ifndef WINDOW_H
#define WINDOW_H

#include <stdio.h>

#include "xtimer.h"
#include "periph/pwm.h"
#include "servo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEV         PWM_DEV(0)
#define CHANNEL     0

#define STEP_LOWER_BOUND (1800U)
#define STEP_UPPER_BOUND (2700U)

int initWindow(servo_t *servo);
void openWindow(servo_t *servo);
void closeWindow(servo_t *servo);


#ifdef __cplusplus
}
#endif

#endif /* WINDOW_H */
/** @} */
