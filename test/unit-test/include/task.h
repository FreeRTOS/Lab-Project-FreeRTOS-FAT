/*
 * Minimal FreeRTOS task.h stub for host-based FreeRTOS+FAT unit tests.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UNIT_TEST_TASK_H
#define UNIT_TEST_TASK_H

#include "FreeRTOS.h"

typedef void * TaskHandle_t;

void vTaskDelay( TickType_t xTicksToDelay );
TaskHandle_t xTaskGetCurrentTaskHandle( void );

#endif /* UNIT_TEST_TASK_H */
