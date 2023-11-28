/*
 * FreeRTOS+FAT V2.3.3
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "ff_headers.h"
#include "event_groups.h"

#ifndef configUSE_RECURSIVE_MUTEXES
    #error configUSE_RECURSIVE_MUTEXES must be set to 1 in FreeRTOSConfig.h
#else
    #if ( configUSE_RECURSIVE_MUTEXES != 1 )
        #error configUSE_RECURSIVE_MUTEXES must be set to 1 in FreeRTOSConfig.h
    #endif
#endif /* configUSE_RECURSIVE_MUTEXES */

#if ( INCLUDE_vTaskDelay != 1 )
    #error Missing some FreeRTOS define
#endif

/* There are two areas which are protected with a semaphore:
 * Directories and the FAT area.
 * The masks below are used when calling Group Event functions. */
#define FF_FAT_LOCK_EVENT_BITS    ( ( const EventBits_t ) FF_FAT_LOCK )
#define FF_DIR_LOCK_EVENT_BITS    ( ( const EventBits_t ) FF_DIR_LOCK )

/* This is not a real lock: it is a bit (or semaphore) will will be given
 * each time when a sector buffer is released. */
#define FF_BUF_LOCK_EVENT_BITS    ( ( const EventBits_t ) FF_BUF_LOCK )

#ifndef FF_TIME_TO_WAIT_FOR_EVENT_TICKS

/* The maximum time to wait for a event group bit to come high,
 * which gives access to a "critical section": either directories,
 * or the FAT. */
    #define FF_TIME_TO_WAIT_FOR_EVENT_TICKS    pdMS_TO_TICKS( 10000UL )
#endif

/*-----------------------------------------------------------*/

BaseType_t FF_TrySemaphore( void * pxSemaphore,
                            uint32_t ulTime_ms )
{
    BaseType_t xReturn;

    /* HT: Actually FF_TrySemaphore is never used. */
    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        return 0;
    }

    configASSERT( pxSemaphore );
    xReturn = xSemaphoreTakeRecursive( ( SemaphoreHandle_t ) pxSemaphore, pdMS_TO_TICKS( ulTime_ms ) );
    return xReturn;
}
/*-----------------------------------------------------------*/

void FF_PendSemaphore( void * pxSemaphore )
{
    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        /* No need to take the semaphore. */
        return;
    }

    configASSERT( pxSemaphore );
    xSemaphoreTakeRecursive( ( SemaphoreHandle_t ) pxSemaphore, portMAX_DELAY );
}
/*-----------------------------------------------------------*/

void FF_ReleaseSemaphore( void * pxSemaphore )
{
    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        /* Scheduler not yet active. */
        return;
    }

    configASSERT( pxSemaphore );
    xSemaphoreGiveRecursive( ( SemaphoreHandle_t ) pxSemaphore );
}
/*-----------------------------------------------------------*/

void FF_Sleep( uint32_t ulTime_ms )
{
    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        /* This sleep is used as a kind of yield.
         * Not necessary while the Scheduler does not run. */
        return;
    }

    vTaskDelay( pdMS_TO_TICKS( ulTime_ms ) );
}
/*-----------------------------------------------------------*/

void FF_DeleteEvents( FF_IOManager_t * pxIOManager )
{
    if( pxIOManager->xEventGroup != NULL )
    {
        vEventGroupDelete( pxIOManager->xEventGroup );
    }
}
/*-----------------------------------------------------------*/

BaseType_t FF_CreateEvents( FF_IOManager_t * pxIOManager )
{
    BaseType_t xResult;

    pxIOManager->xEventGroup = xEventGroupCreate();

    if( pxIOManager->xEventGroup != NULL )
    {
        xEventGroupSetBits( pxIOManager->xEventGroup,
                            FF_FAT_LOCK_EVENT_BITS | FF_DIR_LOCK_EVENT_BITS | FF_BUF_LOCK_EVENT_BITS );
        xResult = pdTRUE;
    }
    else
    {
        xResult = pdFALSE;
    }

    return xResult;
}
/*-----------------------------------------------------------*/

void FF_LockDirectory( FF_IOManager_t * pxIOManager )
{
    /* Called when a task want to make changes to a directory.
     * It waits for the desired bit to come high, and clears the
     * bit so that other tasks can not take it. */

    EventBits_t xBits;

    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        /* Scheduler not yet active. */
        return;
    }

    for( ; ; )
    {
        xEventGroupWaitBits( pxIOManager->xEventGroup,
                             FF_DIR_LOCK_EVENT_BITS, /* uxBitsToWaitFor */
                             pdFALSE,                /* xClearOnExit */
                             pdFALSE,                /* xWaitForAllBits n.a. */
                             FF_TIME_TO_WAIT_FOR_EVENT_TICKS );

        /* At this point, this task is one of many potentially unblocked by
         * xEventGroupSetBits. The next operation will only succeed for 1 task at a
         * time, because it is an atomic test & set operation: */
        xBits = xEventGroupClearBits( pxIOManager->xEventGroup,
                                      FF_DIR_LOCK_EVENT_BITS );

        if( ( xBits & FF_DIR_LOCK_EVENT_BITS ) != 0 )
        {
            /* This task has cleared the desired bit.
             * It now 'owns' the resource. */
            break;
        }
    }
}
/*-----------------------------------------------------------*/

void FF_UnlockDirectory( FF_IOManager_t * pxIOManager )
{
    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        /* Scheduler not yet active. */
        return;
    }

    configASSERT( ( xEventGroupGetBits( pxIOManager->xEventGroup ) & FF_DIR_LOCK_EVENT_BITS ) == 0 );
    xEventGroupSetBits( pxIOManager->xEventGroup, FF_DIR_LOCK_EVENT_BITS );
}
/*-----------------------------------------------------------*/

int FF_Has_Lock( FF_IOManager_t * pxIOManager,
                 uint32_t aBits )
{
    int iReturn;

    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        /* Scheduler not yet active. */
        return 0;
    }

    void * handle = xTaskGetCurrentTaskHandle();

    if( ( aBits & FF_FAT_LOCK_EVENT_BITS ) != 0 )
    {
        if( ( pxIOManager->pvFATLockHandle != NULL ) && ( pxIOManager->pvFATLockHandle == handle ) )
        {
            iReturn = pdTRUE;
        }
        else
        {
            iReturn = pdFALSE;
        }
    }
    else
    {
        iReturn = pdFALSE;
    }

    return iReturn;
}

void FF_Assert_Lock( FF_IOManager_t * pxIOManager,
                     uint32_t aBits )
{
    void * handle;

    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        /* Scheduler not yet active. */
        return;
    }

    handle = xTaskGetCurrentTaskHandle();

    if( ( aBits & FF_FAT_LOCK_EVENT_BITS ) != 0 )
    {
        configASSERT( ( pxIOManager->pvFATLockHandle != NULL ) && ( pxIOManager->pvFATLockHandle == handle ) );
        /* In case configASSERT() is not defined. */
        ( void ) pxIOManager;
        ( void ) handle;
    }
}

void FF_LockFAT( FF_IOManager_t * pxIOManager )
{
    /* Called when a task want to make changes to the FAT area.
     * It waits for the desired bit to come high, and clears the
     * bit so that other tasks can not take it. */

    EventBits_t xBits;

    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        /* Scheduler not yet active. */
        return;
    }

    configASSERT( FF_Has_Lock( pxIOManager, FF_FAT_LOCK ) == pdFALSE );

    for( ; ; )
    {
        xEventGroupWaitBits( pxIOManager->xEventGroup,
                             FF_FAT_LOCK_EVENT_BITS, /* uxBitsToWaitFor */
                             pdFALSE,                /* xClearOnExit */
                             pdFALSE,                /* xWaitForAllBits n.a. */
                             FF_TIME_TO_WAIT_FOR_EVENT_TICKS );

        /* At this point, this task is one of many potentially unblocked by
         * xEventGroupSetBits. The next operation will only succeed for 1 task at a
         * time, because it is an atomic test & set operation: */
        xBits = xEventGroupClearBits( pxIOManager->xEventGroup,
                                      FF_FAT_LOCK_EVENT_BITS );

        if( ( xBits & FF_FAT_LOCK_EVENT_BITS ) != 0 )
        {
            /* This task has cleared the desired bit.
             * It now 'owns' the resource. */
            configASSERT( pxIOManager->pvFATLockHandle == NULL );
            pxIOManager->pvFATLockHandle = xTaskGetCurrentTaskHandle();
            break;
        }
    }
}
/*-----------------------------------------------------------*/

void FF_UnlockFAT( FF_IOManager_t * pxIOManager )
{
    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        /* Scheduler not yet active. */
        return;
    }

    configASSERT( ( xEventGroupGetBits( pxIOManager->xEventGroup ) & FF_FAT_LOCK_EVENT_BITS ) == 0 );
    pxIOManager->pvFATLockHandle = NULL;
    xEventGroupSetBits( pxIOManager->xEventGroup, FF_FAT_LOCK_EVENT_BITS );
}
/*-----------------------------------------------------------*/

BaseType_t FF_BufferWait( FF_IOManager_t * pxIOManager,
                          uint32_t xWaitMS )
{
    EventBits_t xBits;
    BaseType_t xReturn;

    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        /* Scheduler not yet active. */
        return pdTRUE;
    }

    /* This function is called when a task is waiting for a sector buffer
     * to become available.  Each time when a sector buffer becomes available,
     * the bit will be set ( see FF_BufferProceed() here below ). */
    xBits = xEventGroupWaitBits( pxIOManager->xEventGroup,
                                 FF_BUF_LOCK_EVENT_BITS, /* uxBitsToWaitFor */
                                 pdTRUE,                 /* xClearOnExit */
                                 pdFALSE,                /* xWaitForAllBits n.a. */
                                 pdMS_TO_TICKS( xWaitMS ) );

    if( ( xBits & FF_BUF_LOCK_EVENT_BITS ) != 0 )
    {
        xReturn = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

void FF_BufferProceed( FF_IOManager_t * pxIOManager )
{
    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        /* Scheduler not yet active. */
        return;
    }

    /* Wake-up a task that is waiting for a sector buffer to become available. */
    xEventGroupSetBits( pxIOManager->xEventGroup, FF_BUF_LOCK_EVENT_BITS );
}
/*-----------------------------------------------------------*/
