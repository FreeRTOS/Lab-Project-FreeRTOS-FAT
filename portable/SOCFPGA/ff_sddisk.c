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

/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 * Modifications to support SoC FPGA
 */

/* Standard includes. */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
#include "portmacro.h"

/* FreeRTOS+FAT includes. */
#include "ff_sddisk.h"
#include "ff_sys.h"

/*usb includes*/
#include "ff_socfpga.h"
#include "socfpga_sdmmc.h"

/* Tinyusb MSC includes */
#include "msc_app.h"
#include "msc_host.h"

#define sdSIGNATURE                          0x41404342UL
#define sdHUNDRED_64_BIT                     ( 100ull )
#define sdBYTES_PER_MB                       ( 1024ull * 1024ull )
#define sdSECTORS_PER_MB                     ( sdBYTES_PER_MB / 512ull )
#define sdIOMAN_MEM_SIZE                     4096
#define PRINT_INFO 0x0


/* Define a time-out for all DMA transactions in msec. */
#ifndef sdMAX_TIME_TICKS
#define sdMAX_TIME_TICKS    pdMS_TO_TICKS( 2000UL )
#endif

#ifndef sdCARD_DETECT_DEBOUNCE_TIME_MS
/* Debouncing time is applied only after card gets inserted. */
#define sdCARD_DETECT_DEBOUNCE_TIME_MS    ( 5000 )
#endif

#ifndef sdARRAY_SIZE
#define sdARRAY_SIZE( x )    ( int ) ( sizeof( x ) / sizeof( x )[ 0 ] )
#endif

/*-----------------------------------------------------------*/

/*
 * Return pdFALSE if the SD card is not inserted.  This function just reads the
 * value of the GPIO C/D pin.
 */
static BaseType_t prvSDDetect( void );
/*
 * Translate a numeric code like 'SD_TX_UNDERRUN' to a printable string.
 */
static const char* prvSDCodePrintable( uint32_t ulCode );
/*
 * The following 'hook' must be provided by the user of this module.  It will be
 * called from a GPIO ISR after every change.  Note that during the ISR, the
 * value of the GPIO is not stable and it can not be used.  All you can do from
 * this hook is wake-up some task, which will call FF_SDDiskDetect().
 */
extern void vApplicationCardDetectChangeHookFromISR(
        BaseType_t *pxHigherPriorityTaskWoken );
/*
 * Check if the card is present, and if so, print out some info on the card.
 */
static BaseType_t prvSDMMCInit( BaseType_t xDriveNumber );

/*-----------------------------------------------------------*/

uint64_t SectorNum = 0;
typedef struct
{
    /* Only after a card has been inserted, debouncing is necessary. */
    TickType_t xRemainingTime;
    TimeOut_t xTimeOut;
    UBaseType_t bLastPresent :1, bStableSignal :1;
} CardDetect_t;

static SemaphoreHandle_t xSDCardSemaphore = NULL;
static SemaphoreHandle_t xPlusFATMutex = NULL;
/* Remembers if the card is currently considered to be present. */
static BaseType_t xSDCardStatus = pdFALSE;
/* Maintains state for card detection. */
static CardDetect_t xCardDetect;
/*call back process for usb tx starts*/

/*call back process for usb tx ends*/
int32_t FFReadSdmmc( uint8_t *pucBuffer, uint32_t ulSectorNumber,
        uint32_t ulSectorCount, FF_Disk_t *pxDisk )
{
    int32_t lReturnCode = FF_ERR_IOMAN_OUT_OF_BOUNDS_READ | FF_ERRFLAG;

    if ((pxDisk != NULL) && (xSDCardStatus == pdPASS)
            && (pxDisk->ulSignature == sdSIGNATURE)
            && (pxDisk->xStatus.bIsInitialised != pdFALSE)
            && (ulSectorNumber < pxDisk->ulNumberOfSectors)
            && ((pxDisk->ulNumberOfSectors - ulSectorNumber) >= ulSectorCount))
    {
        uint64_t ullReadAddr;
        int32_t sd_result;

        ullReadAddr = 512ull * (uint64_t) ulSectorNumber;
        if ((((size_t) pucBuffer) & (sizeof(size_t) - 1)) == 0)
        {
            sd_result = sdmmc_read_block_sync((uint64_t*) pucBuffer, ullReadAddr,
                    512ul, ulSectorCount);
        }
        else
        {
            uint32_t ulSector;
            uint8_t *pucDMABuffer = ffconfigMALLOC(512ul);

            if (pucDMABuffer != NULL)
            {
                sd_result = 0;
                for (ulSector = 0; ulSector < ulSectorCount; ulSector++)
                {
                    ullReadAddr = 512ull *
                            ((uint64_t) ulSectorNumber + (uint64_t) ulSector);
                    sd_result = sdmmc_read_block_sync((uint64_t*) pucDMABuffer,
                            512 * ulSectorNumber, 512ul, ulSectorCount);

                    if (sd_result == 0)
                    {
                        memcpy(pucBuffer + 512ul * ulSector, pucDMABuffer,
                                512ul);
                    }
                    else
                    {
                        break;
                    }

                }
                ffconfigFREE(pucDMABuffer);
            }
            else
            {
                sd_result = -EIO;
            }
        }

        if (sd_result == 0)
        {
            lReturnCode = 0L;
        }
        else
        {
            FF_PRINTF("prvFFRead: %u: %u (%s)\n", (unsigned) ulSectorNumber,
                    sd_result, prvSDCodePrintable(sd_result));
        }
    }
    else
    {
        memset((void*) pucBuffer, '\0', ulSectorCount * 512UL);

    }

    return lReturnCode;
}



int32_t FFWriteSdmmc( uint8_t *pucBuffer, uint32_t ulSectorNumber,
        uint32_t ulSectorCount, FF_Disk_t *pxDisk )
{
    int32_t lReturnCode = FF_ERR_IOMAN_OUT_OF_BOUNDS_READ | FF_ERRFLAG;

    if ((pxDisk != NULL) && (xSDCardStatus == pdPASS)
            && (pxDisk->ulSignature == sdSIGNATURE)
            && (pxDisk->xStatus.bIsInitialised != pdFALSE)
            && (ulSectorNumber < pxDisk->ulNumberOfSectors)
            && ((pxDisk->ulNumberOfSectors - ulSectorNumber) >= ulSectorCount))
    {
        int32_t sd_result;
        uint64_t ullWriteAddr;
        ullWriteAddr = 512ull * ulSectorNumber;

        if ((((size_t) pucBuffer) & (sizeof(size_t) - 1)) == 0)
        {
            sd_result = sdmmc_write_block_sync((uint64_t*) pucBuffer, ullWriteAddr,
                    512ul, ulSectorCount);
        }
        else
        {
            uint32_t ulSector;
            uint8_t *pucDMABuffer = ffconfigMALLOC(512ul);

            if (pucDMABuffer != NULL)
            {
                sd_result = 0;

                for (ulSector = 0; ulSector < ulSectorCount; ulSector++)
                {
                    memcpy(pucDMABuffer, pucBuffer + 512ul * ulSector, 512ul);
                    ullWriteAddr = 512ull * (ulSectorNumber + ulSector);
                    sd_result = sdmmc_write_block_sync((uint64_t*) pucDMABuffer,
                            ullWriteAddr, 512ul, 1);

                    if (sd_result != 0)
                    {
                        break;
                    }

                }

                ffconfigFREE(pucDMABuffer);
            }
            else
            {
                sd_result = -EIO;
            }
        }

        if (sd_result == 0)
        {
            lReturnCode = 0L;
        }
    }

    return lReturnCode;
}



int32_t FFReadUsb( uint8_t *pucBuffer, uint32_t ulSectorNumber,
        uint32_t ulSectorCount, FF_Disk_t *pxDisk )
{
    int32_t lReturnCode = FF_ERR_IOMAN_OUT_OF_BOUNDS_READ | FF_ERRFLAG;

    if ( (pxDisk != NULL) && (xSDCardStatus == pdPASS)
            && (pxDisk->ulSignature == sdSIGNATURE)
            && (pxDisk->xStatus.bIsInitialised != pdFALSE)
            && (ulSectorNumber < pxDisk->ulNumberOfSectors)
            && ((pxDisk->ulNumberOfSectors - ulSectorNumber) >= ulSectorCount) )
    {
        bool usb_result = false;

        if ( (((size_t) pucBuffer) & (sizeof(size_t) - 1)) == 0 )
        {
            usb_result = usb_disk_read(pucBuffer, ulSectorNumber,(uint16_t)ulSectorCount);
        }
        else
        {
            uint32_t ulSector;
            uint8_t *pucDMABuffer = ffconfigMALLOC(512ul * ulSectorCount);

            if ( pucDMABuffer != NULL )
            {
                usb_result = true;
                for ( ulSector = 0; ulSector < ulSectorCount; ulSector++ )
                {
                    usb_result = usb_disk_read(pucDMABuffer, 512ul * ulSectorNumber, ulSectorCount);

                    if ( usb_result == true )
                    {
                        memcpy(pucBuffer + 512ul * ulSector, pucDMABuffer,
                                512ul * ulSectorCount);
                    }
                    else
                    {
                        break;
                    }

                }
                ffconfigFREE(pucDMABuffer);
            }
            else
            {
                usb_result = false;
            }
        }
        if ( usb_result == true )
        {
            lReturnCode = 0L;
        }
    }
    else
    {
        memset((void*) pucBuffer, '\0', ulSectorCount * 512UL);
    }

    return lReturnCode;
}
/*-----------------------------------------------------------*/

int32_t FFWriteUsb( uint8_t *pucBuffer, uint32_t ulSectorNumber,
        uint32_t ulSectorCount, FF_Disk_t *pxDisk )
{
    int32_t lReturnCode = FF_ERR_IOMAN_OUT_OF_BOUNDS_READ | FF_ERRFLAG;

    if ( (pxDisk != NULL) && (xSDCardStatus == pdPASS)
            && (pxDisk->ulSignature == sdSIGNATURE)
            && (pxDisk->xStatus.bIsInitialised != pdFALSE)
            && (ulSectorNumber < pxDisk->ulNumberOfSectors)
            && ((pxDisk->ulNumberOfSectors - ulSectorNumber) >= ulSectorCount) )
    {
        bool usb_result = false;

        if ( (((size_t) pucBuffer) & (sizeof(size_t) - 1)) == 0 )
        {
        	usb_result = usb_disk_write(pucBuffer, ulSectorNumber, ulSectorCount);
        }
        else
        {
            uint32_t ulSector;
            uint8_t *pucDMABuffer = ffconfigMALLOC(512ul * ulSectorCount);

            if ( pucDMABuffer != NULL )
            {
                usb_result = true;

                for ( ulSector = 0; ulSector < ulSectorCount; ulSector++ )
                {
                    memcpy(pucDMABuffer, pucBuffer + 512ul * ulSector, 512ul * ulSectorCount);
                    usb_result = usb_disk_write( pucDMABuffer, ulSectorNumber, ulSectorCount);
                    if (usb_result != false )
                    {
                        break;
                    }
                }
                ffconfigFREE(pucDMABuffer);
            }
            else
            {
                usb_result = false;
            }
        }

        if ( usb_result == true )
        {
            lReturnCode = 0L;
        }
    }

    return lReturnCode;
}
/*-----------------------------------------------------------*/

void FF_SDDiskFlush( FF_Disk_t *pxDisk )
{
    if ( (pxDisk != NULL) && (pxDisk->xStatus.bIsInitialised != pdFALSE)
            && (pxDisk->pxIOManager != NULL) )
    {
        FF_FlushCache(pxDisk->pxIOManager);
    }
}
/*-----------------------------------------------------------*/

FF_Disk_t* FF_SDDiskInit( const char *pcName , int ulDriveNum)
{
    FF_Error_t xFFError;
    BaseType_t xPartitionNumber = 0;
    FF_CreationParameters_t xParameters;
    FF_Disk_t *pxDisk;

    xSDCardStatus = prvSDMMCInit(ulDriveNum);

    if ( xSDCardStatus != pdPASS )
    {
        printf("\r FF_SDDiskInit: prvSDMMCInit failed\n");
        pxDisk = NULL;
    }
    else
    {
        pxDisk = (FF_Disk_t*) ffconfigMALLOC(sizeof(*pxDisk));

        if ( pxDisk == NULL )
        {
            printf("\r FF_SDDiskInit: Malloc failed\n");
        }
        else
        {
            memset(pxDisk, '\0', sizeof(*pxDisk));

            if ( xPlusFATMutex == NULL )
            {
                xPlusFATMutex = xSemaphoreCreateRecursiveMutex();
            }

            pxDisk->ulSignature = sdSIGNATURE;
            if( ulDriveNum >=0)
            {
            	uint32_t block_count = tuh_msc_get_block_count(1, 0);
            	/* printf("Block count is : %d \r\n", block_count); */
            	pxDisk->ulNumberOfSectors = block_count;
            }
            else
            {
            	pxDisk->ulNumberOfSectors = SectorNum;
            }

            if ( xPlusFATMutex != NULL )
            {
                memset(&xParameters, '\0', sizeof(xParameters));
                xParameters.ulMemorySize = sdIOMAN_MEM_SIZE;
                xParameters.ulSectorSize = 512;
                xParameters.fnWriteBlocks = FatWriteAbsLayer;
                xParameters.fnReadBlocks = FatReadAbsLayer;
                xParameters.pxDisk = pxDisk;

                xParameters.xBlockDeviceIsReentrant = pdFALSE;

                /* The semaphore will be used to protect critical sections in
                 * the +FAT driver, and also to avoid concurrent calls to
                 * prvFFRead()/prvFFWrite() from different tasks. */
                xParameters.pvSemaphore = (void*) xPlusFATMutex;

                pxDisk->pxIOManager = FF_CreateIOManager(&xParameters,
                        &xFFError);

                if ( pxDisk->pxIOManager == NULL )
                {
                    FF_PRINTF("FF_SDDiskInit: FF_CreateIOManager: %s\n",
                            (const char*) FF_GetErrMessage(xFFError));

                    FF_SDDiskDelete(pxDisk);

                    printf("\r Disk deletion\n");
                    pxDisk = NULL;
                }
                else
                {
                    pxDisk->xStatus.bIsInitialised = pdTRUE;
                    pxDisk->xStatus.bPartitionNumber = xPartitionNumber;
                    pxDisk->ulDevAddr = ulDriveNum;

                    if ( FF_SDDiskMount(pxDisk) == 0 )
                    {

                        FF_SDDiskDelete(pxDisk);
                        pxDisk = NULL;
                    }
                    else
                    {
                        if ( pcName == NULL )
                        {
                            pcName = "/";
                        }
                        FF_FS_Add(pcName, pxDisk);
                        FF_SDDiskShowPartition(pxDisk);
                    }
                }
            }
        }
    }

    return pxDisk;
}
/*-----------------------------------------------------------*/

BaseType_t FF_SDDiskFormat( FF_Disk_t *pxDisk, BaseType_t xPartitionNumber )
{
    FF_Error_t xError;
    BaseType_t xReturn = pdFAIL;
    xError = FF_Unmount(pxDisk);

    if ( FF_isERR( xError ) != pdFALSE )
    {
        FF_PRINTF("FF_SDDiskFormat: unmount fails: %08x\n", (unsigned) xError);
    }
    else
    {
        xError = FF_Format(pxDisk, xPartitionNumber, pdFALSE, pdFALSE);

        if ( FF_isERR(xError) )
        {
            FF_PRINTF("FF_SDDiskFormat: %s\n",
                    (const char*) FF_GetErrMessage(xError));
        }
        else
        {
            FF_PRINTF("FF_SDDiskFormat: OK, now remounting\n");
            pxDisk->xStatus.bPartitionNumber = xPartitionNumber;
            xError = FF_SDDiskMount(pxDisk);
            FF_PRINTF("FF_SDDiskFormat: rc %08x\n", (unsigned) xError);

            if ( FF_isERR( xError ) == pdFALSE )
            {
                xReturn = pdPASS;
                FF_SDDiskShowPartition(pxDisk);
            }
        }
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t FF_SDDiskUnmount( FF_Disk_t *pxDisk )
{
    FF_Error_t xFFError;
    BaseType_t xReturn = pdPASS;

    if ( (pxDisk != NULL) && (pxDisk->xStatus.bIsMounted != pdFALSE) )
    {
        pxDisk->xStatus.bIsMounted = pdFALSE;
        xFFError = FF_Unmount(pxDisk);

        if ( FF_isERR(xFFError) )
        {
            FF_PRINTF("FF_SDDiskUnmount: rc %08x\n", (unsigned) xFFError);
            xReturn = pdFAIL;
        }
        else
        {
            FF_PRINTF("Drive unmounted\n");
        }
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t FF_SDDiskReinit( FF_Disk_t *pxDisk )
{
    BaseType_t xStatus = prvSDMMCInit(0);

    (void) pxDisk;

    return xStatus;
}
/*-----------------------------------------------------------*/

BaseType_t FF_SDDiskMount( FF_Disk_t *pxDisk )
{
    FF_Error_t xFFError;
    BaseType_t xReturn;

    xFFError = FF_Mount(pxDisk, pxDisk->xStatus.bPartitionNumber);

    if ( FF_isERR(xFFError) )
    {
        xReturn = pdFAIL;
    }
    else
    {
        pxDisk->xStatus.bIsMounted = pdTRUE;
        xReturn = pdPASS;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

FF_IOManager_t* sddisk_ioman( FF_Disk_t *pxDisk )
{
    FF_IOManager_t *pxReturn;

    if ( (pxDisk != NULL) && (pxDisk->xStatus.bIsInitialised != pdFALSE) )
    {
        pxReturn = pxDisk->pxIOManager;
    }
    else
    {
        pxReturn = NULL;
    }

    return pxReturn;
}
/*-----------------------------------------------------------*/

BaseType_t FF_SDDiskDelete( FF_Disk_t *pxDisk )
{
    if ( pxDisk != NULL )
    {
        pxDisk->ulSignature = 0;
        pxDisk->xStatus.bIsInitialised = 0;

        if ( pxDisk->pxIOManager != NULL )
        {
            if ( FF_Mounted(pxDisk->pxIOManager) != pdFALSE )
            {
                FF_Unmount(pxDisk);
            }

            FF_DeleteIOManager(pxDisk->pxIOManager);
        }

        vPortFree(pxDisk);
    }

    return 1;
}
/*-----------------------------------------------------------*/

BaseType_t FF_SDDiskShowPartition( FF_Disk_t *pxDisk )
{
    FF_Error_t xError;
    uint64_t ullFreeSectors;
    uint32_t ulTotalSizeMB, ulFreeSizeMB;
    int iPercentageFree;
    FF_IOManager_t *pxIOManager;
    const char *pcTypeName = "unknown type";
    BaseType_t xReturn = pdPASS;

    if ( pxDisk == NULL )
    {
        xReturn = pdFAIL;
    }
    else
    {
        pxIOManager = pxDisk->pxIOManager;

        switch (pxIOManager->xPartition.ucType)
        {
            case FF_T_FAT12:
                pcTypeName = "FAT12";
                break;

            case FF_T_FAT16:
                pcTypeName = "FAT16";
                break;

            case FF_T_FAT32:
                pcTypeName = "FAT32";
                break;

            default:
                pcTypeName = "UNKOWN";
                break;
        }

        FF_GetFreeSize(pxIOManager, &xError);

        ullFreeSectors = pxIOManager->xPartition.ulFreeClusterCount
            * pxIOManager->xPartition.ulSectorsPerCluster;
        iPercentageFree = (int) (( sdHUNDRED_64_BIT * ullFreeSectors
                    + pxIOManager->xPartition.ulDataSectors / 2)
                / ((uint64_t) pxIOManager->xPartition.ulDataSectors));

        ulTotalSizeMB =
            pxIOManager->xPartition.ulDataSectors / sdSECTORS_PER_MB;
        ulFreeSizeMB = (uint32_t) (ullFreeSectors / sdSECTORS_PER_MB);
#if PRINT_INFO
        printf( "\r Partition Nr   %8u\n ", pxDisk->xStatus.bPartitionNumber );
        printf( "\r Type           %8u (%s)\n", pxIOManager->xPartition.ucType, pcTypeName );
        printf( "\r VolLabel       '%8s' \n", pxIOManager->xPartition.pcVolumeLabel );
        printf( "\r TotalSectors   %u\n", ( unsigned ) pxIOManager->xPartition.ulTotalSectors );
        printf("\n");
        printf( "\r SecsPerCluster %u ", ( unsigned ) pxIOManager->xPartition.ulSectorsPerCluster );
        fflush(stdout);
        printf("\n");
        printf( "\r Size           %u MB\n", ( unsigned ) ulTotalSizeMB );
        printf( "\r FreeSize       %u MB ( %d percent free )\n", ( unsigned ) ulFreeSizeMB, iPercentageFree );
#endif
    }
    (void) ulTotalSizeMB;
    (void) ulFreeSizeMB;
    (void) iPercentageFree;
    (void) pcTypeName;

    return xReturn;
}
/*-----------------------------------------------------------*/

/* This routine returns true if the SD-card is inserted.  After insertion, it
 * will wait for sdCARD_DETECT_DEBOUNCE_TIME_MS before returning pdTRUE. */

static BaseType_t prvSDDetect( void )
{
    int iReturn;
    if ( sdmmc_is_card_present() == 0 )
    {
        iReturn = pdFALSE;
    }
    else
    {
        iReturn = pdTRUE;
    }

    return iReturn;
}

BaseType_t FF_SDDiskDetect( FF_Disk_t *pxDisk )
{
    (void) pxDisk;
    int xReturn;
    xReturn = prvSDDetect();
    if ( xReturn != pdFALSE )
    {
        if ( xCardDetect.bStableSignal == pdFALSE )
        {
            if ( xCardDetect.bLastPresent == pdFALSE )
            {
                xCardDetect.bLastPresent = pdTRUE;
                xCardDetect.xRemainingTime = pdMS_TO_TICKS(
                        ( TickType_t ) sdCARD_DETECT_DEBOUNCE_TIME_MS);
                vTaskSetTimeOutState(&xCardDetect.xTimeOut);
            }

            if ( xTaskCheckForTimeOut(&xCardDetect.xTimeOut,
                        &xCardDetect.xRemainingTime) != pdFALSE )
            {
                xCardDetect.bStableSignal = pdTRUE;
            }
            else
            {

                xReturn = pdFALSE;
            }
        }
    }
    else
    {
        xCardDetect.bLastPresent = pdFALSE;
        xCardDetect.bStableSignal = pdFALSE;
    }

    return xReturn;
}

static BaseType_t prvSDMMCInit( BaseType_t xDriveNumber )
{
    uint64_t *PtrSectorNum = &SectorNum;
    int SD_state = 0;

    if(xDriveNumber == -1)
    {
        if ( xSDCardSemaphore == NULL )
        {
            xSDCardSemaphore = xSemaphoreCreateBinary();
        }
        if ( prvSDDetect() == pdFALSE )
        {
            printf("No SD card detected\n");
            return 0;
        }
        SD_state = sdmmc_init_card(PtrSectorNum);
        xCardDetect.bLastPresent = pdTRUE;
        xCardDetect.bStableSignal = pdTRUE;
        return SD_state == 0 ? 1 : 0;
    }
    else
    {
    	/*add usb init here*/
        xCardDetect.bLastPresent = pdTRUE;
        xCardDetect.bStableSignal = pdTRUE;
        return pdPASS;
    }


}
/*-----------------------------------------------------------*/

struct xCODE_NAME
{
    uint32_t ulValue;
    const char *pcName;
};

static const char* prvSDCodePrintable( uint32_t ulCode )
{
    (void) ulCode;
    static char retString[ 32 ];
    return retString;
}
