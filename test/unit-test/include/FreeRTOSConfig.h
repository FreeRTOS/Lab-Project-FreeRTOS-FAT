/*
 * Minimal FreeRTOSConfig.h for host-based FreeRTOS+FAT unit tests.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UNIT_TEST_FREERTOS_CONFIG_H
#define UNIT_TEST_FREERTOS_CONFIG_H

#define configUSE_16_BIT_TICKS              0
#define configSUPPORT_DYNAMIC_ALLOCATION    1
#define configSUPPORT_STATIC_ALLOCATION     0

#endif /* UNIT_TEST_FREERTOS_CONFIG_H */
