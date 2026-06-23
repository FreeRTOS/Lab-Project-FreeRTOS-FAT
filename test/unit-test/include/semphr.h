/*
 * Minimal FreeRTOS semphr.h stub for host-based FreeRTOS+FAT unit tests.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UNIT_TEST_SEMPHR_H
#define UNIT_TEST_SEMPHR_H

#include "FreeRTOS.h"

typedef void * SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex( void );
void vSemaphoreDelete( SemaphoreHandle_t xSemaphore );
BaseType_t xSemaphoreTakeRecursive( SemaphoreHandle_t xMutex,
                                    TickType_t xBlockTime );
BaseType_t xSemaphoreGiveRecursive( SemaphoreHandle_t xMutex );

#endif /* UNIT_TEST_SEMPHR_H */
