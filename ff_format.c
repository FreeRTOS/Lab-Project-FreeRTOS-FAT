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

/**
 * @file ff_format.c
 * @ingroup FORMAT
 *
 * @defgroup FAT Fat File-System
 * @brief Format a drive, given the number of sectors.
 *
 **/

#include "ff_headers.h"

#include <time.h>
#include <string.h>

#if defined( __BORLANDC__ )
    #include "ff_windows.h"
#else
    #include "FreeRTOS.h"
    #include "task.h" /* For FreeRTOS date/time function */
#endif


/*=========================================================================================== */

#define OFS_PART_ACTIVE_8               0x000 /* 0x01BE 0x80 if active */
#define OFS_PART_START_HEAD_8           0x001 /* 0x01BF */
#define OFS_PART_START_SEC_TRACK_16     0x002 /* 0x01C0 */
#define OFS_PART_ID_NUMBER_8            0x004 /* 0x01C2 */
#define OFS_PART_ENDING_HEAD_8          0x005 /* 0x01C3 */
#define OFS_PART_ENDING_SEC_TRACK_16    0x006 /* 0x01C4   = SectorCount - 1 - ulHiddenSectors */
#define OFS_PART_STARTING_LBA_32        0x008 /* 0x01C6   = ulHiddenSectors (This is important) */
#define OFS_PART_LENGTH_32              0x00C /* 0x01CA   = SectorCount - 1 - ulHiddenSectors */

#define OFS_PTABLE_MACH_CODE            0x000 /* 0x0000 */
#define OFS_PTABLE_PART_0               0x1BE /* 446 */
#define OFS_PTABLE_PART_1               0x1CE /* 462 */
#define OFS_PTABLE_PART_2               0x1DE /* 478 */
#define OFS_PTABLE_PART_3               0x1FE /* 494 */
#define OFS_PTABLE_PART_LEN             16

/*=========================================================================================== */

#define OFS_BPB_jmpBoot_24             0x000 /* uchar jmpBoot[3] "0xEB 0x00 0x90" */
#define OFS_BPB_OEMName_64             0x003 /* uchar BS_OEMName[8] "MSWIN4.1" */

#define OFS_BPB_BytesPerSec_16         0x00B /* Only 512, 1024, 2048 or 4096 */
#define OFS_BPB_SecPerClus_8           0x00D /* Only 1, 2, 4, 8, 16, 32, 64, 128 */
#define OFS_BPB_ResvdSecCnt_16         0x00E /* ulFATReservedSectors, e.g. 1 (FAT12/16) or 32 (FAT32) */

#define OFS_BPB_NumFATs_8              0x010 /* 2 recommended */
#define OFS_BPB_RootEntCnt_16          0x011 /* ((iFAT16RootSectors * 512) / 32) 512 (FAT12/16) or 0 (FAT32) */
#define OFS_BPB_TotSec16_16            0x013 /* xxx (FAT12/16) or 0 (FAT32) */
#define OFS_BPB_Media_8                0x015 /* 0xF0 (rem media) also in FAT[0] low byte */

#define OFS_BPB_FATSz16_16             0x016
#define OFS_BPB_SecPerTrk_16           0x018 /* n.a. CF has no tracks */
#define OFS_BPB_NumHeads_16            0x01A /* n.a. 1 ? */
#define OFS_BPB_HiddSec_32             0x01C /* n.a. 0 for non-partitioned volume */
#define OFS_BPB_TotSec32_32            0x020 /* >= 0x10000 */

#define OFS_BPB_16_DrvNum_8            0x024 /* n.a. */
#define OFS_BPB_16_Reserved1_8         0x025 /* n.a. */
#define OFS_BPB_16_BootSig_8           0x026 /* n.a. */
#define OFS_BPB_16_BS_VolID_32         0x027 /* "unique" number */
#define OFS_BPB_16_BS_VolLab_88        0x02B /* "NO NAME    " */
#define OFS_BPB_16_FilSysType_64       0x036 /* "FAT12   " */

#define OFS_BPB_32_FATSz32_32          0x024 /* Only when BPB_FATSz16 = 0 */
#define OFS_BPB_32_ExtFlags_16         0x028 /* FAT32 only */
#define OFS_BPB_32_FSVer_16            0x02A /* 0:0 */
#define OFS_BPB_32_RootClus_32         0x02C /* See 'iFAT32RootClusters' Normally 2 */
#define OFS_BPB_32_FSInfo_16           0x030 /* Normally 1 */
#define OFS_BPB_32_BkBootSec_16        0x032 /* Normally 6 */
#define OFS_BPB_32_Reserved_96         0x034 /* Zeros */
#define OFS_BPB_32_DrvNum_8            0x040 /* n.a. */
#define OFS_BPB_32_Reserved1_8         0x041 /* n.a. */
#define OFS_BPB_32_BootSig_8           0x042 /* n.a. */
#define OFS_BPB_32_VolID_32            0x043 /* "unique" number */
#define OFS_BPB_32_VolLab_88           0x047 /* "NO NAME    " */
#define OFS_BPB_32_FilSysType_64       0x052 /* "FAT12   " */

#define OFS_FSI_32_LeadSig             0x000 /* With contents 0x41615252 */
#define OFS_FSI_32_Reserved1           0x004 /* 480 times 0 */
#define OFS_FSI_32_StrucSig            0x1E4 /* With contents 0x61417272 */
#define OFS_FSI_32_Free_Count          0x1E8 /* last known free cluster count on the volume, ~0 for unknown */
#define OFS_FSI_32_Nxt_Free            0x1EC /* cluster number at which the driver should start looking for free clusters */
#define OFS_FSI_32_Reserved2           0x1F0 /* zero's */
#define OFS_FSI_32_TrailSig            0x1FC /* 0xAA550000 (little endian) */

#define RESV_COUNT                     32

#define MX_LBA_TO_MOVE_FAT             8192uL
#define SIZE_512_MB                    0x100000uL

#ifdef ffconfigMIN_CLUSTERS_FAT32
    #define MIN_CLUSTER_COUNT_FAT32    ffconfigMIN_CLUSTERS_FAT32
#else
    #define MIN_CLUSTER_COUNT_FAT32    ( 65525 )
#endif

#ifdef ffconfigMIN_CLUSTERS_FAT16
    #define MIN_CLUSTERS_FAT16    ffconfigMIN_CLUSTERS_FAT16
#else
    #define MIN_CLUSTERS_FAT16    ( 4085 + 1 )
#endif

#ifndef ffconfigFAT16_ROOT_SECTORS
    #define ffconfigFAT16_ROOT_SECTORS    32
#endif

/*-----------------------------------------------------------*/

/** A set of variables needed while formatting a disk, it is passed to the helper functions. */
struct xFormatSet
{
    uint32_t ulHiddenSectors;         /**< Space from MBR ( Master Boot Record ) and partition table. */
    uint32_t ulFSInfo;                /**< Sector number of FSINFO structure within the reserved area. */
    uint32_t ulBackupBootSector;      /**< Sector number of "copy of the boot record" within the reserved area. */
    BaseType_t xFATCount;             /**< Number of FAT's, which is fixed as 2. */
    uint32_t ulFATReservedSectors;    /**< Space between the partition table and FAT table. */
    int32_t iFAT16RootSectors;        /**< Number of sectors reserved for root directory ( FAT16 only ). */
    int32_t iFAT32RootClusters;       /**< Initial amount of clusters claimed for root directory ( FAT32 only ). */
    uint8_t ucFATType;                /**< Either 'FF_T_FAT16' or 'FF_T_FAT32'. */
    uint32_t ulVolumeID;              /**< A pseudo Volume ID. */

    uint32_t ulSectorsPerFAT;         /**< Number of sectors used by a single FAT table. */
    uint32_t ulClustersPerFATSector;  /**< # of clusters which can be described within a sector ( either 256 or 128 ). */
    uint32_t ulSectorsPerCluster;     /**< Size of a cluster ( # of sectors ). */
    uint32_t ulUsableDataSectors;     /**< Usable data sectors ( = SectorCount - ( ulHiddenSectors + ulFATReservedSectors ) ). */
    uint32_t ulUsableDataClusters;    /**< equals "ulUsableDataSectors / ulSectorsPerCluster". */
    uint32_t ulNonDataSectors;        /**< ulFATReservedSectors + ulHiddenSectors + iFAT16RootSectors. */
    uint32_t ulClusterBeginLBA;       /**< Sector address of the first data cluster. */
    uint32_t ulSectorCount;           /**< The total number of sectors in the partition. */
    uint8_t * pucSectorBuffer;        /**< A buffer big enough to contain the contents of one sector ( see pxIOManager->usSectorSize ). */
    FF_SPartFound_t xPartitionsFound; /**< An array of descriptors of partitions. */
    FF_Part_t * pxMyPartition;        /**< A pointer to the partition descriptor for the disk to be formatted. */
    FF_IOManager_t * pxIOManager;     /**< The IO-manager. */
};

/** A set of variables needed while partitioning a disk, it is passed to the helper functions. */
struct xPartitionSet
{
    uint32_t ulInterSpace;                            /**< Hidden space between 2 extended partitions. */
    FF_Part_t pxPartitions[ ffconfigMAX_PARTITIONS ]; /**< A short description of all partitions. */
    BaseType_t xPartitionCount;                       /**< The number of partitions wanted by the caller. */
    FF_IOManager_t * pxIOManager;                     /**< The +FAT IO-manager. */
};

/*-----------------------------------------------------------*/

/* Five helper functions for FF_FormatDisk(). */
static FF_Error_t prvFormatGetClusterSize( struct xFormatSet * pxSet,
                                           BaseType_t xPreferFAT16,
                                           BaseType_t xSmallClusters );

static void prvFormatOptimiseFATLocation( struct xFormatSet * pxSet );

static FF_Error_t prvFormatWriteBPB( struct xFormatSet * pxSet,
                                     const char * pcVolumeName );

static FF_Error_t prvFormatInitialiseFAT( struct xFormatSet * pxSet,
                                          int32_t lFatBeginLBA );

static FF_Error_t prvFormatInitialiseRootDir( struct xFormatSet * pxSet,
                                              int32_t lDirectoryBegin,
                                              const char * pcVolumeName );

/* And two helper functions for FF_Partition(). */
static FF_Error_t prvPartitionPrimary( struct xPartitionSet * pxSet );

static FF_Error_t prvPartitionExtended( struct xPartitionSet * pxSet,
                                        FF_PartitionParameters_t * pParams );

/*-----------------------------------------------------------*/

static portINLINE uint32_t ulMin32( uint32_t a,
                                    uint32_t b )
{
    uint32_t ulReturn;

    if( a <= b )
    {
        ulReturn = a;
    }
    else
    {
        ulReturn = b;
    }

    return ulReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief: Decide whether FAT32 or FAT16 shall be used and try to find an optimum cluster size.
 * @param[in] pxSet A set of parameters describing this format session.
 * @param[in] xPreferFAT16 When pdTRUE, it will use FAT16 in stead of FAT32.
 * @param[in] xSmallClusters When pdTRUE, it will make the cluster size as small as possible.
 * @return A standard +FAT error code ( not an errno ).
 * @note In order to get the best speed, use pdFALSE, pdFALSE: to get FAT32 with large clusters.
 */
static FF_Error_t prvFormatGetClusterSize( struct xFormatSet * pxSet,
                                           BaseType_t xPreferFAT16,
                                           BaseType_t xSmallClusters )
{
    FF_Error_t xReturn = FF_ERR_NONE;

    /* Either search from small to large or v.v. */
    if( xSmallClusters != 0 )
    {
        /* The caller prefers to have small clusters.
         * Less waste but it can be slower. */
        pxSet->ulSectorsPerCluster = 1U;
    }
    else
    {
        if( pxSet->ucFATType == FF_T_FAT32 )
        {
            pxSet->ulSectorsPerCluster = 64U;
        }
        else
        {
            pxSet->ulSectorsPerCluster = 32U;
        }
    }

    for( ; ; )
    {
        uint32_t groupSize;
        /* Usable sectors */
        pxSet->ulUsableDataSectors = pxSet->ulSectorCount - pxSet->ulNonDataSectors;
        /* Each group consists of 'xFATCount' sectors + 'ulClustersPerFATSector' clusters */
        groupSize = ( uint32_t ) pxSet->xFATCount + pxSet->ulClustersPerFATSector * pxSet->ulSectorsPerCluster;
        /* This amount of groups will fit: */
        pxSet->ulSectorsPerFAT = ( uint32_t ) ( ( pxSet->ulUsableDataSectors + groupSize - pxSet->ulSectorsPerCluster - ( uint32_t ) pxSet->xFATCount ) / groupSize );

        pxSet->ulUsableDataClusters = ulMin32(
            ( uint32_t ) ( pxSet->ulUsableDataSectors - pxSet->xFATCount * pxSet->ulSectorsPerFAT ) / pxSet->ulSectorsPerCluster,
            ( uint32_t ) ( pxSet->ulClustersPerFATSector * pxSet->ulSectorsPerFAT ) );
        pxSet->ulUsableDataSectors = pxSet->ulUsableDataClusters * pxSet->ulSectorsPerCluster;

        if( ( pxSet->ucFATType == FF_T_FAT16 ) && ( pxSet->ulUsableDataClusters >= MIN_CLUSTERS_FAT16 ) && ( pxSet->ulUsableDataClusters < 65536U ) )
        {
            break;
        }

        if( ( pxSet->ucFATType == FF_T_FAT32 ) && ( pxSet->ulUsableDataClusters >= 65536U ) && ( pxSet->ulUsableDataClusters < 0x0FFFFFEFU ) )
        {
            break;
        }

        /* Was this the last test? */
        if( ( ( xSmallClusters != pdFALSE ) && ( pxSet->ulSectorsPerCluster == 32U ) ) ||
            ( ( xSmallClusters == pdFALSE ) && ( pxSet->ulSectorsPerCluster == 1U ) ) )
        {
            FF_PRINTF( "FF_Format: Can not make a FAT%d (tried %d) with %u sectors\n",
                       pxSet->ucFATType == FF_T_FAT32 ? 32 : 16,
                       xPreferFAT16 ? 16 : 32,
                       ( unsigned ) pxSet->ulSectorCount );
            xReturn = FF_createERR( FF_ERR_IOMAN_BAD_MEMSIZE, FF_MODULE_FORMAT );
            break;
        }

        /* No it wasn't, try next clustersize */
        if( xSmallClusters != pdFALSE )
        {
            pxSet->ulSectorsPerCluster <<= 1;
        }
        else
        {
            pxSet->ulSectorsPerCluster >>= 1;
        }
    } /* for( ;; ) */

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief: Optimise FAT location: for bigger disks, let the FAT start at 4MB.
 * @param[in] pxSet A set of parameters describing this format session.
 */
static void prvFormatOptimiseFATLocation( struct xFormatSet * pxSet )
{
    if( ( pxSet->ucFATType == FF_T_FAT32 ) &&
        ( pxSet->ulSectorCount >= SIZE_512_MB ) &&
        ( pxSet->pxMyPartition->ulStartLBA < MX_LBA_TO_MOVE_FAT ) )
    {
        uint32_t ulRemaining;

        /*
         * Putting the FAT-table into the second 4MB erase block gives
         * a higher performance and a longer life-time.
         * See e.g. here:
         * http://3gfp.com/wp/2014/07/formatting-sd-cards-for-speed-and-lifetime/
         */
        pxSet->ulFATReservedSectors = MX_LBA_TO_MOVE_FAT - pxSet->ulHiddenSectors;
        pxSet->ulNonDataSectors = pxSet->ulFATReservedSectors + ( uint32_t ) pxSet->iFAT16RootSectors;

        ulRemaining = ( pxSet->ulNonDataSectors + 2U * pxSet->ulSectorsPerFAT ) % 128U;

        if( ulRemaining != 0U )
        {
            /* In order to get ClusterBeginLBA well aligned (on a 128 sector boundary) */
            pxSet->ulFATReservedSectors += ( 128U - ulRemaining );
            pxSet->ulNonDataSectors = pxSet->ulFATReservedSectors + ( uint32_t ) pxSet->iFAT16RootSectors;
        }

        pxSet->ulUsableDataSectors = pxSet->ulSectorCount - pxSet->ulNonDataSectors - 2 * pxSet->ulSectorsPerFAT;
        pxSet->ulUsableDataClusters = pxSet->ulUsableDataSectors / pxSet->ulSectorsPerCluster;
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief: Write the so-called BIOS Parameter Block ( BPB ). It describes the FAT partition.
 * @param[in] pxSet A set of parameters describing this format session.
 * @return A standard +FAT error code.
 */
static FF_Error_t prvFormatWriteBPB( struct xFormatSet * pxSet,
                                     const char * pcVolumeName )
{
    FF_Error_t xReturn;
    char pcName[ 12 ];

    ( void ) strncpy( pcName, pcVolumeName, sizeof( pcName ) - 1 );
    pcName[ sizeof( pcName ) - 1 ] = 0;

    /* Clear all fields that aren't set explicitely. */
    ( void ) memset( pxSet->pucSectorBuffer, 0, pxSet->pxIOManager->usSectorSize );

    ( void ) memcpy( pxSet->pucSectorBuffer + OFS_BPB_jmpBoot_24, "\xEB\x00\x90" "FreeRTOS", 11 );                      /* Includes OFS_BPB_OEMName_64 */

    FF_putShort( pxSet->pucSectorBuffer, OFS_BPB_BytesPerSec_16, pxSet->pxIOManager->usSectorSize );                    /* 0x00B / Only 512, 1024, 2048 or 4096 */
    FF_putShort( pxSet->pucSectorBuffer, OFS_BPB_ResvdSecCnt_16, ( uint32_t ) pxSet->ulFATReservedSectors );            /*  0x00E / 1 (FAT12/16) or 32 (FAT32) */

    FF_putChar( pxSet->pucSectorBuffer, OFS_BPB_NumFATs_8, 2 );                                                         /* 0x010 / 2 recommended */
    FF_putShort( pxSet->pucSectorBuffer, OFS_BPB_RootEntCnt_16, ( uint32_t ) ( pxSet->iFAT16RootSectors * 512 ) / 32 ); /* 0x011 / 512 (FAT12/16) or 0 (FAT32) */

    /* For FAT12 and FAT16 volumes, this field contains the count of 32- */
    /* byte directory entries in the root directory */

    FF_putChar( pxSet->pucSectorBuffer, OFS_BPB_Media_8, 0xF8 );                                         /* 0x015 / 0xF0 (rem media) also in FAT[0] low byte */

    FF_putShort( pxSet->pucSectorBuffer, OFS_BPB_SecPerTrk_16, 0x3F );                                   /* 0x18 n.a. CF has no tracks */
    FF_putShort( pxSet->pucSectorBuffer, OFS_BPB_NumHeads_16, 255 );                                     /* 0x01A / n.a. 1 ? */
    FF_putLong( pxSet->pucSectorBuffer, OFS_BPB_HiddSec_32, ( uint32_t ) pxSet->ulHiddenSectors );       /* 0x01C / n.a. 0 for non-partitioned volume */

    FF_putChar( pxSet->pucSectorBuffer, OFS_BPB_SecPerClus_8, ( uint32_t ) pxSet->ulSectorsPerCluster ); /*  0x00D / Only 1, 2, 4, 8, 16, 32, 64, 128 */
    FF_PRINTF( "FF_Format: SecCluster %u DatSec %u DataClus %u pxSet->ulClusterBeginLBA %lu\n",
               ( unsigned ) pxSet->ulSectorsPerCluster, ( unsigned ) pxSet->ulUsableDataSectors, ( unsigned ) pxSet->ulUsableDataClusters, pxSet->ulClusterBeginLBA );

    /* This field is the new 32-bit total count of sectors on the volume. */
    /* This count includes the count of all sectors in all four regions of the volume */
    FF_putShort( pxSet->pucSectorBuffer, OFS_BPB_TotSec16_16, 0 );                   /* 0x013 / xxx (FAT12/16) or 0 (FAT32) */

    FF_putLong( pxSet->pucSectorBuffer, OFS_BPB_TotSec32_32, pxSet->ulSectorCount ); /* 0x020 / >= 0x10000 */

    if( pxSet->ucFATType == FF_T_FAT32 )
    {
        FF_putLong( pxSet->pucSectorBuffer, OFS_BPB_32_FATSz32_32, pxSet->ulSectorsPerFAT );                  /* 0x24 / Only when BPB_FATSz16 = 0 */
        FF_putShort( pxSet->pucSectorBuffer, OFS_BPB_32_ExtFlags_16, 0 );                                     /* 0x28 / FAT32 only */
        FF_putShort( pxSet->pucSectorBuffer, OFS_BPB_32_FSVer_16, 0 );                                        /* 0x2A / 0:0 */
        FF_putLong( pxSet->pucSectorBuffer, OFS_BPB_32_RootClus_32, ( uint32_t ) pxSet->iFAT32RootClusters ); /* 0x2C / Normally 2 */
        FF_putShort( pxSet->pucSectorBuffer, OFS_BPB_32_FSInfo_16, pxSet->ulFSInfo );                         /* 0x30 / Normally 1 */
        FF_putShort( pxSet->pucSectorBuffer, OFS_BPB_32_BkBootSec_16, pxSet->ulBackupBootSector );            /* 0x32 / Normally 6 */
        FF_putChar( pxSet->pucSectorBuffer, OFS_BPB_32_DrvNum_8, 0 );                                         /* 0x40 / n.a. */
        FF_putChar( pxSet->pucSectorBuffer, OFS_BPB_32_BootSig_8, 0x29 );                                     /* 0x42 / n.a. */
        FF_putLong( pxSet->pucSectorBuffer, OFS_BPB_32_VolID_32, ( uint32_t ) pxSet->ulVolumeID );            /* 0x43 / "unique" number */
        ( void ) memcpy( pxSet->pucSectorBuffer + OFS_BPB_32_VolLab_88, pcName, 11 );                         /* 0x47 / "NO NAME    " */
        ( void ) memcpy( pxSet->pucSectorBuffer + OFS_BPB_32_FilSysType_64, "FAT32   ", 8 );                  /* 0x52 / "FAT12   " */
    }
    else
    {
        FF_putChar( pxSet->pucSectorBuffer, OFS_BPB_16_DrvNum_8, 0u );                                /* 0x024 / n.a. */
        FF_putChar( pxSet->pucSectorBuffer, OFS_BPB_16_Reserved1_8, 0 );                              /* 0x025 / n.a. */
        FF_putChar( pxSet->pucSectorBuffer, OFS_BPB_16_BootSig_8, 0x29 );                             /* 0x026 / n.a. */
        FF_putLong( pxSet->pucSectorBuffer, OFS_BPB_16_BS_VolID_32, ( uint32_t ) pxSet->ulVolumeID ); /* 0x027 / "unique" number */

        FF_putShort( pxSet->pucSectorBuffer, OFS_BPB_FATSz16_16, pxSet->ulSectorsPerFAT );            /* 0x16 */

        ( void ) memcpy( pxSet->pucSectorBuffer + OFS_BPB_16_BS_VolLab_88, "MY NAME    ", 11 );       /* 0x02B / "NO NAME    " */
        ( void ) memcpy( pxSet->pucSectorBuffer + OFS_BPB_16_FilSysType_64, "FAT16   ", 8 );          /* 0x036 / "FAT12   " */
    }

    pxSet->pucSectorBuffer[ FF_FAT_MBR_SIGNATURE + 0 ] = 0x55;
    pxSet->pucSectorBuffer[ FF_FAT_MBR_SIGNATURE + 1 ] = 0xAA;

    xReturn = FF_BlockWrite( pxSet->pxIOManager, pxSet->ulHiddenSectors, 1, pxSet->pucSectorBuffer, 0u );

    if( ( pxSet->ucFATType == FF_T_FAT32 ) &&
        ( FF_isERR( xReturn ) == pdFALSE ) )
    {
        /* Store a backup copy of the boot sector. */
        xReturn = FF_BlockWrite( pxSet->pxIOManager, pxSet->ulHiddenSectors + pxSet->ulBackupBootSector, 1, pxSet->pucSectorBuffer, pdFALSE );
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief: Initialise and clear the File Allocation Table ( FAT ).
 * @param[in] pxSet A set of parameters describing this format session.
 * @param[in] lFatBeginLBA The number of first sector of the FAT.
 * @return A standard +FAT error code.
 */
static FF_Error_t prvFormatInitialiseFAT( struct xFormatSet * pxSet,
                                          int32_t lFatBeginLBA )
{
    FF_Error_t xReturn;

    ( void ) memset( pxSet->pucSectorBuffer, 0, pxSet->pxIOManager->usSectorSize );

    switch( pxSet->ucFATType )
    {
        case FF_T_FAT16:
            FF_putShort( pxSet->pucSectorBuffer, 0, 0xFFF8U ); /* First FAT entry. */
            FF_putShort( pxSet->pucSectorBuffer, 2, 0xFFFFU ); /* RESERVED alloc. */
            break;

        case FF_T_FAT32:
            FF_putLong( pxSet->pucSectorBuffer, 0, 0x0FFFFFF8U ); /* FAT32 FAT sig. */
            FF_putLong( pxSet->pucSectorBuffer, 4, 0xFFFFFFFFU ); /* RESERVED alloc. */
            FF_putLong( pxSet->pucSectorBuffer, 8, 0x0FFFFFFFU ); /* Root dir allocation. */
            break;

        default:
            break;
    }

    xReturn = FF_BlockWrite( pxSet->pxIOManager, ( uint32_t ) lFatBeginLBA, 1, pxSet->pucSectorBuffer, pdFALSE );

    if( FF_isERR( xReturn ) == pdFALSE )
    {
        xReturn = FF_BlockWrite( pxSet->pxIOManager, ( uint32_t ) lFatBeginLBA + pxSet->ulSectorsPerFAT, 1, pxSet->pucSectorBuffer, pdFALSE );
    }

    FF_PRINTF( "FF_Format: Clearing entire FAT (2 x %u sectors):\n", ( unsigned ) pxSet->ulSectorsPerFAT );
    {
        int32_t addr;

        ( void ) memset( pxSet->pucSectorBuffer, 0, pxSet->pxIOManager->usSectorSize );

        for( addr = lFatBeginLBA + 1;
             ( addr < ( lFatBeginLBA + ( int32_t ) pxSet->ulSectorsPerFAT ) ) &&
             ( FF_isERR( xReturn ) == pdFALSE );
             addr++ )
        {
            xReturn = FF_BlockWrite( pxSet->pxIOManager, ( uint32_t ) addr, 1, pxSet->pucSectorBuffer, pdFALSE );

            if( FF_isERR( xReturn ) == pdFALSE )
            {
                xReturn = FF_BlockWrite( pxSet->pxIOManager, ( uint32_t ) addr + pxSet->ulSectorsPerFAT, 1, pxSet->pucSectorBuffer, pdFALSE );
            }
        }
    }
    FF_PRINTF( "FF_Format: Clearing done\n" );

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief: Initialise and clear the root directory.
 * @param[in] pxSet A set of parameters describing this format session.
 * @return A standard +FAT error code.
 */
static FF_Error_t prvFormatInitialiseRootDir( struct xFormatSet * pxSet,
                                              int32_t lDirectoryBegin,
                                              const char * pcVolumeName )
{
    FF_Error_t xReturn = FF_ERR_NONE;
    int32_t lAddress;
    int32_t lLastAddress;
    BaseType_t xHasCleared = pdFALSE;

    ( void ) memset( pxSet->pucSectorBuffer, 0, pxSet->pxIOManager->usSectorSize );
    ( void ) memcpy( pxSet->pucSectorBuffer, pcVolumeName, 11 );
    pxSet->pucSectorBuffer[ 11 ] = FF_FAT_ATTR_VOLID;

    if( pxSet->iFAT16RootSectors != 0 )
    {
        lLastAddress = lDirectoryBegin + pxSet->iFAT16RootSectors;
    }
    else
    {
        lLastAddress = lDirectoryBegin + ( int32_t ) pxSet->ulSectorsPerCluster;
    }

    FF_PRINTF( "FF_Format: Clearing root directory at %08lX: %lu sectors\n", lDirectoryBegin, lLastAddress - lDirectoryBegin );

    for( lAddress = lDirectoryBegin;
         ( lAddress < lLastAddress ) && ( FF_isERR( xReturn ) == pdFALSE );
         lAddress++ )
    {
        xReturn = FF_BlockWrite( pxSet->pxIOManager, ( uint32_t ) lAddress, 1, pxSet->pucSectorBuffer, 0u );

        if( xHasCleared == pdFALSE )
        {
            xHasCleared = pdTRUE;
            ( void ) memset( pxSet->pucSectorBuffer, 0, pxSet->pxIOManager->usSectorSize );
        }
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief Now deprecated, please use the new function 'FF_FormatDisk()'.
 * @param[in] pxDisk The disk object.
 * @param[in] xPartitionNumber the numer of the partitioned that must be FAT-formatted.
 * @param[in] xPreferFAT16 When pdTRUE, it will use FAT16 in stead of FAT32.
 * @param[in] xSmallClusters When pdTRUE, it will make the cluster size as small as possible.
 * @return A standard +FAT error code ( not an errno ).
 */
FF_Error_t FF_Format( FF_Disk_t * pxDisk,
                      BaseType_t xPartitionNumber,
                      BaseType_t xPreferFAT16,
                      BaseType_t xSmallClusters )
{
    return FF_FormatDisk( pxDisk, xPartitionNumber, xPreferFAT16, xSmallClusters, "MY_DISK    " );
}
/*-----------------------------------------------------------*/

/*_RB_ Candidate for splitting into multiple functions? */

/**
 * @brief Format a partition of a disk, either as FAT16 or FAT32. It assumes that
 *        is has already been partitioned.
 * @param[in] pxDisk The disk object.
 * @param[in] xPartitionNumber the numer of the partitioned that must be FAT-formatted.
 * @param[in] xPreferFAT16 When pdTRUE, it will use FAT16 in stead of FAT32.
 * @param[in] xSmallClusters When pdTRUE, it will make the cluster size as small as possible.
 * @param[in] pcVolumeName A  string of 11 characters representing the name of the disk.
 * @return A standard +FAT error code ( not an errno ).
 */
FF_Error_t FF_FormatDisk( FF_Disk_t * pxDisk,
                          BaseType_t xPartitionNumber,
                          BaseType_t xPreferFAT16,
                          BaseType_t xSmallClusters,
                          const char * pcVolumeName )
{
    struct xFormatSet xSet;
    int32_t lFatBeginLBA;
    int32_t lDirectoryBegin;
    FF_Error_t xReturn = FF_ERR_NONE;

    memset( &( xSet ), 0, sizeof xSet );

    xSet.ulFSInfo = 1;           /* Sector number of FSINFO structure within the reserved area */
    xSet.ulBackupBootSector = 6; /* Sector number of "copy of the boot record" within the reserved area */
    xSet.xFATCount = 2;          /* Number of FAT's */
    xSet.pxIOManager = pxDisk->pxIOManager;

    FF_PartitionSearch( xSet.pxIOManager, &xSet.xPartitionsFound );

    /* Introducing a do {} while(false) loop for easy exit without return. */
    do
    {
        if( xPartitionNumber >= xSet.xPartitionsFound.iCount )
        {
            xReturn = FF_createERR( FF_ERR_IOMAN_INVALID_PARTITION_NUM, FF_MODULE_FORMAT );
            break;
        }

        xSet.pxMyPartition = xSet.xPartitionsFound.pxPartitions + xPartitionNumber;
        xSet.ulSectorCount = xSet.pxMyPartition->ulSectorCount;

        xSet.ulHiddenSectors = xSet.pxMyPartition->ulStartLBA;

        if( ( ( xPreferFAT16 == pdFALSE ) && ( ( xSet.ulSectorCount - RESV_COUNT ) >= 65536 ) ) ||
            ( ( xSet.ulSectorCount - RESV_COUNT ) >= ( 64 * MIN_CLUSTER_COUNT_FAT32 ) ) )
        {
            xSet.ucFATType = FF_T_FAT32;
            xSet.iFAT32RootClusters = 2;
            xSet.ulFATReservedSectors = RESV_COUNT;
            xSet.iFAT16RootSectors = 0;
        }
        else
        {
            xSet.ucFATType = FF_T_FAT16;
            xSet.iFAT32RootClusters = 0;
            xSet.ulFATReservedSectors = 1u;
            xSet.iFAT16RootSectors = ffconfigFAT16_ROOT_SECTORS; /* 32 sectors to get 512 dir entries */
        }

        /* Set start sector and length to allow FF_BlockRead/Write */
        xSet.pxIOManager->xPartition.ulTotalSectors = xSet.pxMyPartition->ulSectorCount;
        xSet.pxIOManager->xPartition.ulBeginLBA = xSet.pxMyPartition->ulStartLBA;

        /* TODO: Find some solution here to get a unique disk ID */
        xSet.ulVolumeID = ( ( uint32_t ) rand() << 16 ) | ( uint32_t ) rand(); /*_RB_ rand() has proven problematic in some environments. */

        /* Sectors within partition which can not be used */
        xSet.ulNonDataSectors = xSet.ulFATReservedSectors + ( uint32_t ) xSet.iFAT16RootSectors;

        /* A fs dependent constant: */
        if( xSet.ucFATType == FF_T_FAT32 )
        {
            /* In FAT32, 4 bytes are needed to store the address (LBA) of a cluster.
             * A FAT sector of 512 bytes can contain 512 / 4 = 128 entries. */
            xSet.ulClustersPerFATSector = xSet.pxIOManager->usSectorSize / sizeof( uint32_t );
        }
        else
        {
            /* In FAT16, 2 bytes are needed to store the address (LBA) of a cluster.
             * A FAT sector of 512 bytes can contain 512 / 2 = 256 entries. */
            xSet.ulClustersPerFATSector = xSet.pxIOManager->usSectorSize / sizeof( uint16_t );
        }

        FF_PRINTF( "FF_Format: Secs %u Rsvd %u Hidden %u Root %u Data %u\n",
                   ( unsigned ) xSet.ulSectorCount, ( unsigned ) xSet.ulFATReservedSectors, ( unsigned ) xSet.ulHiddenSectors,
                   ( unsigned ) xSet.iFAT16RootSectors, ( unsigned ) xSet.ulSectorCount - xSet.ulNonDataSectors );

        /*****************************/

        /* Try to find the optimum sector size, and choose betwee FAT16 and FAT32.
         */
        xReturn = prvFormatGetClusterSize( &( xSet ), xPreferFAT16, xSmallClusters );

        if( FF_isERR( xReturn ) != pdFALSE )
        {
            break;
        }

        /*****************************/

        /* Optimise FAT location: for bigger disks, let the FAT start at an offset of 4MB,
         * because that memory is optimised for FAT purposes ( i.e. frequent changes ).
         */
        prvFormatOptimiseFATLocation( &( xSet ) );

        xSet.ulClusterBeginLBA = xSet.ulHiddenSectors + xSet.ulFATReservedSectors + 2 * xSet.ulSectorsPerFAT;

        /* Allocate a buffer space to hold a full sector. */
        xSet.pucSectorBuffer = ( uint8_t * ) ffconfigMALLOC( xSet.pxIOManager->usSectorSize );

        if( xSet.pucSectorBuffer == NULL )
        {
            xReturn = FF_createERR( FF_ERR_NOT_ENOUGH_MEMORY, FF_MODULE_FORMAT );
            break;
        }

        /*****************************/

        /* Write the so-called BIOS parameter block (BPB). It describes the FAT partition. */
        xReturn = prvFormatWriteBPB( &( xSet ), pcVolumeName );

        if( FF_isERR( xReturn ) != pdFALSE )
        {
            break;
        }

        /*****************************/

        if( xSet.ucFATType == FF_T_FAT32 )
        {
            /* FAT32 stores extra information in the FSInfo sector, usually sector 1. */
            ( void ) memset( xSet.pucSectorBuffer, 0, xSet.pxIOManager->usSectorSize );

            FF_putLong( xSet.pucSectorBuffer, OFS_FSI_32_LeadSig, 0x41615252 );                   /* to validate that this is in fact an FSInfo sector. */
            /* OFS_FSI_32_Reserved1  0x004 / 480 times 0 */
            FF_putLong( xSet.pucSectorBuffer, OFS_FSI_32_StrucSig, 0x61417272 );                  /* Another signature that is more localized in the */
            /* sector to the location of the fields that are used. */
            FF_putLong( xSet.pucSectorBuffer, OFS_FSI_32_Free_Count, xSet.ulUsableDataClusters ); /* last known free cluster count on the volume, ~0 for unknown */
            FF_putLong( xSet.pucSectorBuffer, OFS_FSI_32_Nxt_Free, 2 );                           /* cluster number at which the driver should start looking for free clusters */
            /* OFS_FSI_32_Reserved2  0x1F0 / zero's */
            FF_putLong( xSet.pucSectorBuffer, OFS_FSI_32_TrailSig, 0xAA550000 );                  /* Will correct for endianness */

            FF_BlockWrite( xSet.pxIOManager, xSet.ulHiddenSectors + xSet.ulFSInfo, 1, xSet.pucSectorBuffer, pdFALSE );
            FF_BlockWrite( xSet.pxIOManager, xSet.ulHiddenSectors + xSet.ulFSInfo + xSet.ulBackupBootSector, 1, xSet.pucSectorBuffer, pdFALSE );
        }

        /*****************************/
        lFatBeginLBA = ( int32_t ) ( xSet.ulHiddenSectors + xSet.ulFATReservedSectors );

        /* Initialise the FAT. */
        xReturn = prvFormatInitialiseFAT( &( xSet ), lFatBeginLBA );

        if( FF_isERR( xReturn ) != pdFALSE )
        {
            break;
        }

        /*****************************/
        lDirectoryBegin = lFatBeginLBA + ( int32_t ) ( 2 * xSet.ulSectorsPerFAT );
        #if ( ffconfigTIME_SUPPORT != 0 )
        {
            FF_SystemTime_t str_t;
            uint16_t myShort;

            FF_GetSystemTime( &str_t );

            myShort = ( ( uint16_t ) ( str_t.Hour << 11U ) & 0xF800U ) |
                      ( ( uint16_t ) ( str_t.Minute << 5U ) & 0x07E0U ) |
                      ( ( uint16_t ) ( str_t.Second / 2U ) & 0x001FU );
            FF_putShort( xSet.pucSectorBuffer, 22, ( uint32_t ) myShort );

            myShort = ( ( uint16_t ) ( ( str_t.Year - 1980 ) << 9U ) & 0xFE00U ) |
                      ( ( uint16_t ) ( str_t.Month << 5U ) & 0x01E0U ) |
                      ( ( uint16_t ) ( str_t.Day & 0x001FU ) );
            FF_putShort( xSet.pucSectorBuffer, 24, ( uint32_t ) myShort );
        }
        #endif /* ffconfigTIME_SUPPORT */

        /* Initialise and clear the root directory. */
        xReturn = prvFormatInitialiseRootDir( &( xSet ), lDirectoryBegin, pcVolumeName );
    }
    while( pdFALSE );

    /* Free the sector buffer. */
    ffconfigFREE( xSet.pucSectorBuffer );

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief Create primary and extended partitions.
 * @param[in] pxSet A set of parameters describing this format session.
 * @return A standard +FAT error code.
 */
static FF_Error_t prvPartitionPrimary( struct xPartitionSet * pxSet )
{
    FF_Error_t xReturn = FF_ERR_NONE;
    FF_Buffer_t * pxSectorBuffer;
    uint8_t * pucBuffer;
    uint32_t ulPartitionOffset; /* Pointer within partition table */
    BaseType_t xPartitionNumber;

    pxSectorBuffer = FF_GetBuffer( pxSet->pxIOManager, 0, ( uint8_t ) FF_MODE_WRITE );
    {
        if( pxSectorBuffer == NULL )
        {
            return FF_ERR_DEVICE_DRIVER_FAILED;
        }
    }

    pucBuffer = pxSectorBuffer->pucBuffer;
    ( void ) memset( pucBuffer, 0, pxSet->pxIOManager->usSectorSize );
    ( void ) memcpy( pucBuffer + OFS_BPB_jmpBoot_24, "\xEB\x00\x90" "FreeRTOS", 11 ); /* Includes OFS_BPB_OEMName_64 */
    ulPartitionOffset = OFS_PTABLE_PART_0;

    for( xPartitionNumber = 0; xPartitionNumber < ffconfigMAX_PARTITIONS; xPartitionNumber++ )
    {
        FF_putChar( pucBuffer, ulPartitionOffset + OFS_PART_ACTIVE_8, pxSet->pxPartitions[ xPartitionNumber ].ucActive );                  /* 0x01BE 0x80 if active */
        FF_putChar( pucBuffer, ulPartitionOffset + OFS_PART_START_HEAD_8, 1 );                                                             /* 0x001 / 0x01BF */
        FF_putShort( pucBuffer, ulPartitionOffset + OFS_PART_START_SEC_TRACK_16, 1 );                                                      /* 0x002 / 0x01C0 */
        FF_putChar( pucBuffer, ulPartitionOffset + OFS_PART_ID_NUMBER_8, pxSet->pxPartitions[ xPartitionNumber ].ucPartitionID );          /* 0x004 / 0x01C2 */
        FF_putChar( pucBuffer, ulPartitionOffset + OFS_PART_ENDING_HEAD_8, 0xFE );                                                         /* 0x005 / 0x01C3 */
        FF_putShort( pucBuffer, ulPartitionOffset + OFS_PART_ENDING_SEC_TRACK_16, pxSet->pxPartitions[ xPartitionNumber ].ulSectorCount ); /* 0x006 / 0x01C4 */
        FF_putLong( pucBuffer, ulPartitionOffset + OFS_PART_STARTING_LBA_32, pxSet->pxPartitions[ xPartitionNumber ].ulStartLBA );         /* 0x008 / 0x01C6 This is important */
        FF_putLong( pucBuffer, ulPartitionOffset + OFS_PART_LENGTH_32, pxSet->pxPartitions[ xPartitionNumber ].ulSectorCount );            /* 0x00C / 0x01CA Equal to total sectors */
        ulPartitionOffset += 16;
    }

    pucBuffer[ FF_FAT_MBR_SIGNATURE + 0 ] = 0x55;
    pucBuffer[ FF_FAT_MBR_SIGNATURE + 1 ] = 0xAA;

    FF_ReleaseBuffer( pxSet->pxIOManager, pxSectorBuffer );
    FF_FlushCache( pxSet->pxIOManager );

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief Create primary and extended partitions.
 * @param[in] pxSet A set of parameters describing this format session.
 * @param[in] pParams A set of variables describing the partitions.
 * @return A standard +FAT error code.
 */
static FF_Error_t prvPartitionExtended( struct xPartitionSet * pxSet,
                                        FF_PartitionParameters_t * pParams )
{
    /* Create at least 1 extended/logical partition */
    FF_Error_t xReturn = FF_ERR_NONE;
    int index;
    /* Start of the big extended partition */
    unsigned extendedLBA = pParams->ulHiddenSectors;
    /* Where to write the table */
    uint32_t ulLBA = 0;

    /* Contents of the table. There must be space for 4 primary,
     * and 4 logical partitions. */
    FF_Part_t writeParts[ 8 ];
    BaseType_t xPartitionNumber;
    FF_Buffer_t * pxSectorBuffer;
    uint8_t * pucBuffer;
    uint32_t ulPartitionOffset; /**< Pointer within partition table */

    for( index = -1; index < pxSet->xPartitionCount; index++ )
    {
        uint32_t ulNextLBA;

        ( void ) memset( writeParts, 0, sizeof( writeParts ) );

        if( index < 0 )
        {
            /* We're at sector 0: */
            /* Write primary partitions, if any */
            /* Create big extended partition */
            uint32_t ulStartLBA = pParams->ulHiddenSectors;

            for( xPartitionNumber = 0; xPartitionNumber < pParams->xPrimaryCount; xPartitionNumber++ )
            {
                writeParts[ xPartitionNumber ].ulStartLBA = ulStartLBA;
                writeParts[ xPartitionNumber ].ulSectorCount = pxSet->pxPartitions[ xPartitionNumber ].ulSectorCount;
                writeParts[ xPartitionNumber ].ucActive = 0x80;
                writeParts[ xPartitionNumber ].ucPartitionID = 0x0B;
                ulStartLBA += writeParts[ xPartitionNumber ].ulSectorCount;
                index++;
            }

            /* _HT_ index = 0, 1, or 2. index < xPrimaryCount. */
            /* _HT_ xPartitionNumber == pParams->xPrimaryCount. */
            /* Now define the extended partition. */
            extendedLBA = ulStartLBA;
            writeParts[ xPartitionNumber ].ulStartLBA = ulStartLBA;
            writeParts[ xPartitionNumber ].ulSectorCount = pParams->ulSectorCount - ulStartLBA;
            writeParts[ xPartitionNumber ].ucActive = 0x80;
            writeParts[ xPartitionNumber ].ucPartitionID = FF_DOS_EXT_PART; /* 0x05 */
            ulNextLBA = ulStartLBA;
        }
        else
        {
            /* Create a logical partition with "ulSectorCount" sectors: */
            writeParts[ 0 ].ulStartLBA = pxSet->ulInterSpace;
            writeParts[ 0 ].ulSectorCount = pxSet->pxPartitions[ index ].ulSectorCount;
            writeParts[ 0 ].ucActive = 0x80;
            writeParts[ 0 ].ucPartitionID = 0x0B;

            if( index < pxSet->xPartitionCount - 1 )
            {
                /* Next extended partition */
                writeParts[ 1 ].ulStartLBA = pxSet->ulInterSpace + ulLBA - extendedLBA + writeParts[ 0 ].ulSectorCount;
                /* Compilers may warn about the out-of-bounds indexing 'pxSet->pxPartitions[index+1]'. */
                writeParts[ 1 ].ulSectorCount = pxSet->pxPartitions[ index + 1 ].ulSectorCount + pxSet->ulInterSpace;
                writeParts[ 1 ].ucActive = 0x80;
                writeParts[ 1 ].ucPartitionID = 0x05;
            }

            ulNextLBA = writeParts[ 1 ].ulStartLBA + extendedLBA;
        }

        pxSectorBuffer = FF_GetBuffer( pxSet->pxIOManager, ( uint32_t ) ulLBA, ( uint8_t ) FF_MODE_WRITE );
        {
            if( pxSectorBuffer == NULL )
            {
                return FF_ERR_DEVICE_DRIVER_FAILED;
            }
        }
        pucBuffer = pxSectorBuffer->pucBuffer;
        ( void ) memset( pucBuffer, 0, pxSet->pxIOManager->usSectorSize );
        ( void ) memcpy( pucBuffer + OFS_BPB_jmpBoot_24, "\xEB\x00\x90" "FreeRTOS", 11 ); /* Includes OFS_BPB_OEMName_64 */

        ulPartitionOffset = OFS_PTABLE_PART_0;

        for( xPartitionNumber = 0; xPartitionNumber < ffconfigMAX_PARTITIONS; xPartitionNumber++, ulPartitionOffset += 16 )
        {
            FF_putChar( pucBuffer, ulPartitionOffset + OFS_PART_ACTIVE_8, writeParts[ xPartitionNumber ].ucActive );                  /* 0x01BE 0x80 if active */
            FF_putChar( pucBuffer, ulPartitionOffset + OFS_PART_START_HEAD_8, 1 );                                                    /* 0x001 / 0x01BF */
            FF_putShort( pucBuffer, ulPartitionOffset + OFS_PART_START_SEC_TRACK_16, 1 );                                             /* 0x002 / 0x01C0 */
            FF_putChar( pucBuffer, ulPartitionOffset + OFS_PART_ID_NUMBER_8, writeParts[ xPartitionNumber ].ucPartitionID );          /* 0x004 / 0x01C2 */
            FF_putChar( pucBuffer, ulPartitionOffset + OFS_PART_ENDING_HEAD_8, 0xFE );                                                /* 0x005 / 0x01C3 */
            FF_putShort( pucBuffer, ulPartitionOffset + OFS_PART_ENDING_SEC_TRACK_16, writeParts[ xPartitionNumber ].ulSectorCount ); /* 0x006 / 0x01C4 */
            FF_putLong( pucBuffer, ulPartitionOffset + OFS_PART_STARTING_LBA_32, writeParts[ xPartitionNumber ].ulStartLBA );         /* 0x008 / 0x01C6 This is important */
            FF_putLong( pucBuffer, ulPartitionOffset + OFS_PART_LENGTH_32, writeParts[ xPartitionNumber ].ulSectorCount );            /* 0x00C / 0x01CA Equal to total sectors */
        }

        pucBuffer[ FF_FAT_MBR_SIGNATURE + 0 ] = 0x55;
        pucBuffer[ FF_FAT_MBR_SIGNATURE + 1 ] = 0xAA;

        FF_ReleaseBuffer( pxSet->pxIOManager, pxSectorBuffer );
        FF_FlushCache( pxSet->pxIOManager );
        ulLBA = ulNextLBA;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief Create/initialise the partitions of a disk.
 * @param[in] pxDisk  The definition of the disk.
 * @param[in] pParams  A description of how the partitions shall be formatted.
 * @return A standard +FAT error code ( not an errno ).
 */
FF_Error_t FF_Partition( FF_Disk_t * pxDisk,
                         FF_PartitionParameters_t * pParams )
{
    FF_Error_t xReturn = FF_ERR_NONE;
    struct xPartitionSet xSet;
    BaseType_t xNeedExtended;    /* When more than 4 partitions are requested, extended partitions are needed. */
    uint32_t ulAvailable;        /* The number of sectors available. */
    BaseType_t xPartitionNumber;
    uint32_t ulSummedSizes = 0U; /* Summed sizes as a percentage or as number of sectors. */
    uint32_t ulReservedSpace;    /**< Space needed for the extended partitions. */

    memset( &( xSet ), 0, sizeof( xSet ) );

    /* Hidden space between 2 extended partitions */
    xSet.ulInterSpace = pParams->ulInterSpace ? pParams->ulInterSpace : 2048;
    /* The +FAT IO-manager. */
    xSet.pxIOManager = pxDisk->pxIOManager;


    /* Clear caching without flushing first. */
    FF_IOMAN_InitBufferDescriptors( xSet.pxIOManager );

    /* Avoid sanity checks by FF_BlockRead/Write. */
    xSet.pxIOManager->xPartition.ulTotalSectors = 0;

    /* Get the sum of sizes and number of actual partitions. */
    for( xPartitionNumber = 0; xPartitionNumber < ffconfigMAX_PARTITIONS; xPartitionNumber++ )
    {
        if( pParams->xSizes[ xPartitionNumber ] > 0 )
        {
            xSet.xPartitionCount++;
            ulSummedSizes += pParams->xSizes[ xPartitionNumber ];
        }
    }

    /* xSet.xPartitionCount is at most 'ffconfigMAX_PARTITIONS' */
    if( xSet.xPartitionCount == 0 )
    {
        xSet.xPartitionCount = 1;

        /* 'ffconfigMAX_PARTITIONS' must be 1 or more. */
        if( pParams->eSizeType == eSizeIsSectors )
        {
            pParams->xSizes[ 0 ] = pParams->ulSectorCount;
        }
        else
        {
            pParams->xSizes[ 0 ] = 100;
        }

        ulSummedSizes = ( uint32_t ) pParams->xSizes[ 0 ];
    }

    /* Correct PrimaryCount if necessary. */
    if( pParams->xPrimaryCount > ( ( xSet.xPartitionCount > 4 ) ? 3 : xSet.xPartitionCount ) )
    {
        pParams->xPrimaryCount = ( xSet.xPartitionCount > 4 ) ? 3 : xSet.xPartitionCount;
    }

    /* Now see if extended is necessary. */
    xNeedExtended = ( xSet.xPartitionCount > pParams->xPrimaryCount ) ? pdTRUE : pdFALSE;

    if( xNeedExtended != pdFALSE )
    {
        if( pParams->ulHiddenSectors < 4096 )
        {
            pParams->ulHiddenSectors = 4096;
        }

        ulReservedSpace = xSet.ulInterSpace * ( uint32_t ) ( xSet.xPartitionCount - pParams->xPrimaryCount );
    }
    else
    {
        /* There must be at least 1 hidden sector. */
        if( pParams->ulHiddenSectors < 1 )
        {
            pParams->ulHiddenSectors = 1;
        }

        ulReservedSpace = 0;
    }

    ulAvailable = pParams->ulSectorCount - pParams->ulHiddenSectors - ulReservedSpace;

    /* Check validity of Sizes */
    switch( pParams->eSizeType )
    {
        case eSizeIsQuota: /* Assign a quotum (sum of Sizes is free, all disk space will be allocated) */
            break;

        case eSizeIsPercent: /* Assign a percentage of the available space (sum of Sizes must be <= 100) */

            if( ulSummedSizes > 100 )
            {
                return FF_createERR( FF_ERR_IOMAN_BAD_MEMSIZE, FF_FORMATPARTITION );
            }

            ulSummedSizes = 100;
            break;

        case eSizeIsSectors: /* Assign fixed number of sectors (512 byte each) */

            if( ulSummedSizes > ulAvailable )
            {
                return FF_createERR( FF_ERR_IOMAN_BAD_MEMSIZE, FF_FORMATPARTITION );
            }

            break;
    }

    {
        uint32_t ulRemaining = ulAvailable;
        uint32_t ulLBA = pParams->ulHiddenSectors;

        /* Divide the available sectors among the partitions: */

        for( xPartitionNumber = 0; xPartitionNumber < xSet.xPartitionCount; xPartitionNumber++ )
        {
            if( pParams->xSizes[ xPartitionNumber ] > 0 )
            {
                uint32_t ulSize;

                switch( pParams->eSizeType )
                {
                    case eSizeIsQuota:   /* Assign a quotum (sum of Sizes is free, all disk space will be allocated) */
                    case eSizeIsPercent: /* Assign a percentage of the available space (sum of Sizes must be <= 100) */
                        ulSize = ( uint32_t ) ( ( ( uint64_t ) pParams->xSizes[ xPartitionNumber ] * ulAvailable ) / ulSummedSizes );
                        break;

                    case eSizeIsSectors: /* Assign fixed number of sectors (512 byte each) */
                    default:             /* Just for the compiler(s) */
                        ulSize = ( uint32_t ) pParams->xSizes[ xPartitionNumber ];
                        break;
                }

                if( ulSize > ulRemaining )
                {
                    ulSize = ulRemaining;
                }

                ulRemaining -= ulSize;
                xSet.pxPartitions[ xPartitionNumber ].ulSectorCount = ulSize;
                xSet.pxPartitions[ xPartitionNumber ].ucActive = 0x80;
                xSet.pxPartitions[ xPartitionNumber ].ulStartLBA = ulLBA; /* ulStartLBA might still change for logical partitions */
                xSet.pxPartitions[ xPartitionNumber ].ucPartitionID = 0x0B;
                ulLBA += ulSize;
            }
        }
    }

    if( xNeedExtended == pdFALSE )
    {
        xReturn = prvPartitionPrimary( &( xSet ) );
    }
    else
    {
        xReturn = prvPartitionExtended( &( xSet ), pParams );
    }

    return xReturn;
}
/*-----------------------------------------------------------*/
