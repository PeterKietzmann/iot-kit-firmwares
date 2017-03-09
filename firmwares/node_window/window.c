/*
 * Copyright (C) 2016 HAW Hamburg
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

#include "window.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

int initWindow(servo_t *servo) {

    return servo_init(servo, DEV, CHANNEL, STEP_LOWER_BOUND, STEP_UPPER_BOUND);
}


void openWindow(servo_t *servo){
    servo_set(servo,STEP_LOWER_BOUND);
}

void closeWindow(servo_t *servo) {
    servo_set(servo, STEP_UPPER_BOUND);
}