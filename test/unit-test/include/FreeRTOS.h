/*
 * Minimal FreeRTOS kernel stub for host-based unit testing of FreeRTOS+FAT.
 *
 * SPDX-License-Identifier: MIT
 *
 * This is NOT the real FreeRTOS kernel. It provides just enough types and
 * macros for the FreeRTOS+FAT sources to compile and run on a host so that
 * algorithmic logic (such as partition-table parsing) can be unit tested
 * without a running scheduler.
 */

#ifndef UNIT_TEST_FREERTOS_H
#define UNIT_TEST_FREERTOS_H

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include "FreeRTOSConfig.h"

/* ProjDefs equivalents. */
typedef long            BaseType_t;
typedef unsigned long   UBaseType_t;
typedef uint32_t        TickType_t;
typedef uint32_t        StackType_t;

#ifndef pdTRUE
    #define pdTRUE     ( ( BaseType_t ) 1 )
#endif
#ifndef pdFALSE
    #define pdFALSE    ( ( BaseType_t ) 0 )
#endif
#ifndef pdPASS
    #define pdPASS     ( pdTRUE )
#endif
#ifndef pdFAIL
    #define pdFAIL     ( pdFALSE )
#endif

/* Endian markers used by FreeRTOSFATConfigDefaults.h. These are also defined
 * in FreeRTOS_errno_FAT.h; guard so both inclusion orders are safe. */
#ifndef pdFREERTOS_LITTLE_ENDIAN
    #define pdFREERTOS_LITTLE_ENDIAN    0
    #define pdFREERTOS_BIG_ENDIAN       1
#endif

#ifndef portMAX_DELAY
    #define portMAX_DELAY    ( ( TickType_t ) 0xffffffffUL )
#endif

#ifndef portINLINE
    #define portINLINE    __inline
#endif

#ifndef pdMS_TO_TICKS
    #define pdMS_TO_TICKS( xTimeInMs )    ( ( TickType_t ) ( xTimeInMs ) )
#endif

#ifndef configASSERT
    #define configASSERT( x )    assert( x )
#endif

/* Critical-section macros become no-ops on the host. */
#define taskENTER_CRITICAL()    do {} while( 0 )
#define taskEXIT_CRITICAL()     do {} while( 0 )
#define taskYIELD()             do {} while( 0 )
#define portYIELD()             do {} while( 0 )

#endif /* UNIT_TEST_FREERTOS_H */
