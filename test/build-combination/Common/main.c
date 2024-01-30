/*
 * FreeRTOS+FAT <DEVELOPMENT BRANCH>
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
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
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/**
 * @file main.c
 * @brief Implements the main function.
 */

/* FreeRTOS include. */
#include <FreeRTOS.h>
#include "task.h"

/* System application includes. */
#include "FreeRTOSConfig.h"
#include "ff_headers.h"
#include "ff_stdio.h"
#include "ff_ramdisk.h"

/* The number and size of sectors that will make up the RAM disk.  The RAM disk
 * is huge to allow some verbose FTP tests used in previous demos. */
#define mainRAM_DISK_SECTOR_SIZE     512UL                                                    /* Currently fixed! */
#define mainRAM_DISK_SECTORS         ( ( 5UL * 1024UL * 1024UL ) / mainRAM_DISK_SECTOR_SIZE ) /* 5M bytes. */
#define mainIO_MANAGER_CACHE_SIZE    ( 15UL * mainRAM_DISK_SECTOR_SIZE )

/* Where the RAM disk is mounted. */
#define mainRAM_DISK_NAME            "/ram"

/*-----------------------------------------------------------*/
int main( void )
{
    /* Initialize the FAT RamDisk */
    static uint8_t ucRAMDisk[ mainRAM_DISK_SECTORS * mainRAM_DISK_SECTOR_SIZE ];
    FF_Disk_t * pxDisk;

    /* Create the RAM disk. */
    pxDisk = FF_RAMDiskInit( mainRAM_DISK_NAME, ucRAMDisk, mainRAM_DISK_SECTORS, mainIO_MANAGER_CACHE_SIZE );
    configASSERT( pxDisk );

    vTaskStartScheduler();

    return 0;
}
/*-----------------------------------------------------------*/
#if ( configUSE_IDLE_HOOK != 0 )

    void vApplicationIdleHook( void )
    {
        /* Exit. Just a stub. */
    }
#endif
/*-----------------------------------------------------------*/

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )

    void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                        StackType_t ** ppxIdleTaskStackBuffer,
                                        configSTACK_DEPTH_TYPE * puxIdleTaskStackSize )
    {
        /* Provide a stub for this function. */
        ( void ) ppxIdleTaskTCBBuffer;
        ( void ) ppxIdleTaskStackBuffer;
        ( void ) puxIdleTaskStackSize;
    }
#endif /* if ( configSUPPORT_STATIC_ALLOCATION == 1 ) */

/*-----------------------------------------------------------*/
#if  ( configUSE_TICK_HOOK != 0 )
    void vApplicationTickHook( void )
    {
        /* Provide a stub for this function. */
    }
#endif

/*-----------------------------------------------------------*/
#if  ( configUSE_DAEMON_TASK_STARTUP_HOOK != 0 )
    void vApplicationDaemonTaskStartupHook( void )
    {
        /* Provide a stub for this function. */
    }
#endif

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )

/*
 * Callback that provides the inputs necessary to generate a randomized TCP
 * Initial Sequence Number per RFC 6528.  THIS IS ONLY A DUMMY IMPLEMENTATION
 * THAT RETURNS A PSEUDO RANDOM NUMBER SO IS NOT INTENDED FOR USE IN PRODUCTION
 * SYSTEMS.
 */
    void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                         StackType_t ** ppxTimerTaskStackBuffer,
                                         uint32_t * pulTimerTaskStackSize )
    {
        /* Provide a stub for this function. */
        ( void ) ppxTimerTaskTCBBuffer;
        ( void ) ppxTimerTaskStackBuffer;
        ( void ) pulTimerTaskStackSize;
    }
#endif /* if ( configSUPPORT_STATIC_ALLOCATION == 1 ) */

void vApplicationMallocFailedHook( void )
{
    /* Provide a stub for this function. */
}
