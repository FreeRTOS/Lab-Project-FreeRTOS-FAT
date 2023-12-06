/*
 * FreeRTOS+FAT Labs Build 160919 (C) 2016 Real Time Engineers ltd.
 * Authors include James Walmsley, Hein Tibosch and Richard Barry
 *
 *
 * FreeRTOS+FAT can be used under two different free open source licenses.  The
 * license that applies is dependent on the processor on which FreeRTOS+FAT is
 * executed, as follows:
 *
 * If FreeRTOS+FAT is executed on one of the processors listed under the Special
 * License Arrangements heading of the FreeRTOS+FAT license information web
 * page, then it can be used under the terms of the FreeRTOS Open Source
 * License.  If FreeRTOS+FAT is used on any other processor, then it can be used
 * under the terms of the GNU General Public License V2.  Links to the relevant
 * licenses follow:
 *
 * The FreeRTOS+FAT License Information Page: http://www.FreeRTOS.org/fat_license
 * The FreeRTOS Open Source License: http://www.FreeRTOS.org/license
 * The GNU General Public License Version 2: http://www.FreeRTOS.org/gpl-2.0.txt
 *
 * FreeRTOS+FAT is distributed in the hope that it will be useful.  You cannot
 * use FreeRTOS+FAT unless you agree that you use the software 'as is'.
 * FreeRTOS+FAT is provided WITHOUT ANY WARRANTY; without even the implied
 * warranties of NON-INFRINGEMENT, MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. Real Time Engineers Ltd. disclaims all conditions and terms, be they
 * implied, expressed, or statutory.
 *
 * This is a stub for allowing linux to compile. should just make it save to a file.
 *
 *
 */

#include "ff_sddisk.h"

/* Standard includes. */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "portmacro.h"

/* FreeRTOS+FAT includes. */
#include "ff_headers.h"
#include "ff_sys.h"

/* Dummy variables. */
#define HUNDRED_64_BIT    ( 100U )
#define BYTES_PER_MB      ( 1024U * 1024U )
#define SECTORS_PER_MB    ( BYTES_PER_MB / 512U )

/*-----------------------------------------------------------*/

BaseType_t FF_SDDiskDetect( FF_Disk_t * pxDisk )
{
    ( void ) pxDisk; /* Unused */
    return pdTRUE;
}

/*-----------------------------------------------------------*/

void FF_SDDiskFlush( FF_Disk_t * pxDisk )
{
    if( ( pxDisk != NULL ) &&
        ( pxDisk->xStatus.bIsInitialised != pdFALSE ) &&
        ( pxDisk->pxIOManager != NULL ) )
    {
        /*flush cache*/
    }
}
/*-----------------------------------------------------------*/

FF_Disk_t * FF_SDDiskInitWithSettings( const char * pcName,
                                       const FFInitSettings_t * pxSettings )
{
    ( void ) pxSettings; /* Unused */

    return FF_SDDiskInit( pcName );
}
/*-----------------------------------------------------------*/

FF_Disk_t * FF_SDDiskInit( const char * pcName )
{
    FF_Disk_t * pxDisk = NULL;

    ( void ) pcName; /* Unused */

    pxDisk = ( FF_Disk_t * ) pvPortMalloc( sizeof( *pxDisk ) );

    if( pxDisk == NULL )
    {
        FF_PRINTF( "FF_SDDiskInit: Malloc failed\n" );
        return NULL;
    }

    /* Initialise the created disk structure. */
    memset( pxDisk, '\0', sizeof( *pxDisk ) );

    /* Other stuff here*/

    pxDisk->xStatus.bIsInitialised = pdTRUE;

    return pxDisk;
}
/*-----------------------------------------------------------*/

BaseType_t FF_SDDiskFormat( FF_Disk_t * pxDisk,
                            BaseType_t aPart )
{
    ( void ) aPart; /* Unused */

    if( pxDisk != NULL )
    {
        FF_PRINTF( "FF_SDDISKFormat \n" );
        return pdPASS;
    }

    return pdFAIL;
}
/*-----------------------------------------------------------*/

/* Unmount the volume */
BaseType_t FF_SDDiskUnmount( FF_Disk_t * pxDisk )
{
    if( ( pxDisk != NULL ) && ( pxDisk->xStatus.bIsMounted != pdFALSE ) )
    {
        pxDisk->xStatus.bIsMounted = pdFALSE;
        FF_PRINTF( "FF_SDDiskUnmount: Drive unmounted\n" );
        return pdPASS;
    }

    return pdFAIL;
}
/*-----------------------------------------------------------*/

BaseType_t FF_SDDiskReinit( FF_Disk_t * pxDisk )
{
    ( void ) pxDisk;
    FF_PRINTF( "FF_SDDiskReinit: rc %08x\n", 0U );
    return pdPASS;
}
/*-----------------------------------------------------------*/

BaseType_t FF_SDDiskMount( FF_Disk_t * pxDisk )
{
    if( pxDisk != NULL )
    {
        pxDisk->xStatus.bIsMounted = pdTRUE;
        FF_PRINTF( "****** FreeRTOS+FAT initialized %u sectors\n", ( unsigned ) pxDisk->pxIOManager->xPartition.ulTotalSectors );
        return pdPASS;
    }

    return pdFAIL;
}
/*-----------------------------------------------------------*/

/* Get a pointer to IOMAN, which can be used for all FreeRTOS+FAT functions */
FF_IOManager_t * sddisk_ioman( FF_Disk_t * pxDisk )
{
    FF_IOManager_t * pxReturn;

    if( ( pxDisk != NULL ) && ( pxDisk->xStatus.bIsInitialised != pdFALSE ) )
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

/* Release all resources */
BaseType_t FF_SDDiskDelete( FF_Disk_t * pxDisk )
{
    if( pxDisk != NULL )
    {
        vPortFree( pxDisk );
    }

    return pdTRUE;
}
/*-----------------------------------------------------------*/

BaseType_t FF_SDDiskShowPartition( FF_Disk_t * pxDisk )
{
    FF_Error_t xError;
    uint64_t ullFreeSectors;
    uint32_t ulTotalSizeMB, ulFreeSizeMB;
    int iPercentageFree;
    FF_IOManager_t * pxIOManager;
    const char * pcTypeName = "unknown type";
    BaseType_t xReturn = pdPASS;

    if( pxDisk == NULL )
    {
        xReturn = pdFAIL;
    }
    else
    {
        pxIOManager = pxDisk->pxIOManager;

        FF_PRINTF( "Reading FAT and calculating Free Space\n" );

        switch( pxIOManager->xPartition.ucType )
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

        FF_GetFreeSize( pxIOManager, &xError );

        ullFreeSectors = pxIOManager->xPartition.ulFreeClusterCount * pxIOManager->xPartition.ulSectorsPerCluster;
        iPercentageFree = ( int ) ( ( HUNDRED_64_BIT * ullFreeSectors + pxIOManager->xPartition.ulDataSectors / 2 ) /
                                    ( ( uint64_t ) pxIOManager->xPartition.ulDataSectors ) );

        ulTotalSizeMB = pxIOManager->xPartition.ulDataSectors / SECTORS_PER_MB;
        ulFreeSizeMB = ( uint32_t ) ( ullFreeSectors / SECTORS_PER_MB );

        /* It is better not to use the 64-bit format such as %Lu because it
         * might not be implemented. */
        FF_PRINTF( "Partition Nr   %8u\n", pxDisk->xStatus.bPartitionNumber );
        FF_PRINTF( "Type           %8s (%u)\n", pcTypeName, pxIOManager->xPartition.ucType );
        FF_PRINTF( "VolLabel       '%8s' \n", pxIOManager->xPartition.pcVolumeLabel );
        FF_PRINTF( "TotalSectors   %8u x 512 = %u\n",
                   ( unsigned ) pxIOManager->xPartition.ulTotalSectors,
                   ( unsigned ) pxIOManager->xPartition.ulTotalSectors * 512U );
        FF_PRINTF( "DataSectors    %8u\n", ( unsigned ) pxIOManager->xPartition.ulDataSectors );
        FF_PRINTF( "SecsPerCluster %8u\n", ( unsigned ) pxIOManager->xPartition.ulSectorsPerCluster );
        FF_PRINTF( "Size           %8u MB\n", ( unsigned ) ulTotalSizeMB );
        FF_PRINTF( "FreeSize       %8u MB ( %d percent free )\n", ( unsigned ) ulFreeSizeMB, ( int ) iPercentageFree );
        FF_PRINTF( "BeginLBA       %8u\n", ( unsigned ) pxIOManager->xPartition.ulBeginLBA );
        FF_PRINTF( "FATBeginLBA    %8u\n", ( unsigned ) pxIOManager->xPartition.ulFATBeginLBA );
    }

    return xReturn;
}

/*-----------------------------------------------------------*/

BaseType_t FF_SDDiskInserted( BaseType_t xDriveNr )
{
    ( void ) xDriveNr;
    return pdFALSE;
}
