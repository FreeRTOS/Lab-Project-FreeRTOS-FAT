/*
 * Minimal FreeRTOS event_groups.h stub for host-based FreeRTOS+FAT unit tests.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UNIT_TEST_EVENT_GROUPS_H
#define UNIT_TEST_EVENT_GROUPS_H

#include "FreeRTOS.h"

typedef void         * EventGroupHandle_t;
typedef TickType_t   EventBits_t;

EventGroupHandle_t xEventGroupCreate( void );
void vEventGroupDelete( EventGroupHandle_t xEventGroup );
EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup,
                                const EventBits_t uxBitsToSet );
EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup,
                                  const EventBits_t uxBitsToClear );
EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup,
                                 const EventBits_t uxBitsToWaitFor,
                                 const BaseType_t xClearOnExit,
                                 const BaseType_t xWaitForAllBits,
                                 TickType_t xTicksToWait );
EventBits_t xEventGroupGetBits( EventGroupHandle_t xEventGroup );

#endif /* UNIT_TEST_EVENT_GROUPS_H */
