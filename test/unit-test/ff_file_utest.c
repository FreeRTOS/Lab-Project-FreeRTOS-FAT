/*
 * Unit tests for file-close metadata updates in ff_file.c.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "mock_ff_locking.h"
#include "mock_ff_dir_mock_subset.h"
#include "mock_ff_fat.h"
#include "mock_ff_ioman_mock_subset.h"
#include "mock_ff_time.h"

#include "ff_headers.h"

#define TEST_SECTOR_SIZE            ( 512U )
#define TEST_SECTORS_PER_CLUSTER    ( 4U )
#define TEST_DIR_ENTRY              ( 3U )
#define TEST_DIR_CLUSTER            ( 9U )
#define TEST_FILE_SIZE              ( 123U )
#define TEST_UPDATED_YEAR           ( 2026U )
#define TEST_DATA_CLUSTER           ( 2U )
#define TEST_DATA_LBA               ( 100U )

static FF_IOManager_t xIOManager;

static void prvInitFile( FF_FILE * pxFile )
{
    memset( &xIOManager, 0, sizeof( xIOManager ) );
    memset( pxFile, 0, sizeof( *pxFile ) );

    xIOManager.usSectorSize = TEST_SECTOR_SIZE;
    xIOManager.xPartition.usBlkSize = TEST_SECTOR_SIZE;
    xIOManager.xPartition.ulSectorsPerCluster = TEST_SECTORS_PER_CLUSTER;
    xIOManager.FirstFile = pxFile;

    pxFile->pxIOManager = &xIOManager;
    pxFile->ulFileSize = TEST_FILE_SIZE;
    pxFile->ucMode = FF_MODE_WRITE;
    pxFile->usDirEntry = TEST_DIR_ENTRY;
    pxFile->ulDirCluster = TEST_DIR_CLUSTER;
}

static void prvExpectValidHandleCheck( void )
{
    FF_PendSemaphore_Expect( xIOManager.pvSemaphore );
    FF_ReleaseSemaphore_Expect( xIOManager.pvSemaphore );
}

static void prvExpectCloseListRemoval( void )
{
    FF_PendSemaphore_Expect( xIOManager.pvSemaphore );
    FF_ReleaseSemaphore_Expect( xIOManager.pvSemaphore );
}

void setUp( void )
{
}

void tearDown( void )
{
}

void test_FF_Write_marks_file_modified_when_a_later_block_write_fails( void )
{
    FF_FILE xFile;
    FF_Buffer_t xBuffer;
    uint8_t pucSector[ TEST_SECTOR_SIZE ];
    uint8_t pucData[ TEST_SECTOR_SIZE * 2U ];
    FF_Error_t xWriteError = FF_createERR( FF_ERR_DEVICE_DRIVER_FAILED, FF_WRITE );

    prvInitFile( &xFile );
    memset( &xBuffer, 0, sizeof( xBuffer ) );
    memset( pucData, 0x5a, sizeof( pucData ) );
    xBuffer.pucBuffer = pucSector;

    xIOManager.xPartition.ucBlkFactor = 1U;
    xFile.ulFilePointer = 1U;
    xFile.ulFileSize = TEST_SECTOR_SIZE * 2U;
    xFile.ulObjectCluster = TEST_DATA_CLUSTER;
    xFile.ulAddrCurrentCluster = TEST_DATA_CLUSTER;
    xFile.ulChainLength = 1U;

    prvExpectValidHandleCheck();
    FF_getMinorBlockEntry_ExpectAndReturn( &xIOManager, 1U, 1U, 1U );
    FF_getClusterChainNumber_ExpectAndReturn( &xIOManager, 1U, 1U, 0U );
    FF_Cluster2LBA_ExpectAndReturn( &xIOManager, TEST_DATA_CLUSTER, TEST_DATA_LBA );
    FF_getMajorBlockNumber_ExpectAndReturn( &xIOManager, 1U, 1U, 0U );
    FF_getMinorBlockNumber_ExpectAndReturn( &xIOManager, 1U, 1U, 0U );
    FF_GetBuffer_ExpectAndReturn( &xIOManager, TEST_DATA_LBA, FF_MODE_WRITE, &xBuffer );
    FF_ReleaseBuffer_ExpectAndReturn( &xIOManager, &xBuffer, FF_ERR_NONE );
    FF_getClusterPosition_ExpectAndReturn( &xIOManager, TEST_SECTOR_SIZE, 1U, TEST_SECTOR_SIZE );
    FF_getClusterChainNumber_ExpectAndReturn( &xIOManager, TEST_SECTOR_SIZE, 1U, 0U );
    FF_Cluster2LBA_ExpectAndReturn( &xIOManager, TEST_DATA_CLUSTER, TEST_DATA_LBA );
    FF_getMajorBlockNumber_ExpectAndReturn( &xIOManager, TEST_SECTOR_SIZE, 1U, 1U );
    FF_getMinorBlockNumber_ExpectAndReturn( &xIOManager, TEST_SECTOR_SIZE, 1U, 0U );
    FF_BlockWrite_ExpectAndReturn( &xIOManager,
                                   TEST_DATA_LBA + 1U,
                                   1U,
                                   &pucData[ TEST_SECTOR_SIZE - 1U ],
                                   pdFALSE,
                                   xWriteError );

    TEST_ASSERT_EQUAL( xWriteError, FF_Write( &xFile, 1U, sizeof( pucData ), pucData ) );
    TEST_ASSERT_BITS_HIGH( FF_VALID_FLAG_MODIFIED, xFile.ulValidFlags );
}

void test_FF_Write_marks_file_modified_when_cluster_write_precedes_traversal_error( void )
{
    FF_FILE xFile;
    uint8_t pucData[ TEST_SECTOR_SIZE * TEST_SECTORS_PER_CLUSTER * 2U ];
    FF_Error_t xTraverseError = FF_createERR( FF_ERR_IOMAN_DRIVER_FATAL_ERROR, FF_WRITECLUSTERS );

    prvInitFile( &xFile );
    memset( pucData, 0x5a, sizeof( pucData ) );

    xFile.ulFileSize = sizeof( pucData );
    xFile.ulObjectCluster = TEST_DATA_CLUSTER;
    xFile.ulAddrCurrentCluster = TEST_DATA_CLUSTER;
    xFile.ulChainLength = 3U;
    xIOManager.xPartition.ucBlkFactor = 1U;

    prvExpectValidHandleCheck();
    FF_getMinorBlockEntry_ExpectAndReturn( &xIOManager, 0U, 1U, 0U );
    FF_getClusterChainNumber_ExpectAndReturn( &xIOManager, 0U, 1U, 0U );
    FF_Cluster2LBA_ExpectAndReturn( &xIOManager, TEST_DATA_CLUSTER, TEST_DATA_LBA );
    FF_getMajorBlockNumber_ExpectAndReturn( &xIOManager, 0U, 1U, 0U );
    FF_getMinorBlockNumber_ExpectAndReturn( &xIOManager, 0U, 1U, 0U );
    FF_getClusterPosition_ExpectAndReturn( &xIOManager, 0U, 1U, 0U );
    FF_getClusterChainNumber_ExpectAndReturn( &xIOManager, 0U, 1U, 0U );
    FF_Cluster2LBA_ExpectAndReturn( &xIOManager, TEST_DATA_CLUSTER, TEST_DATA_LBA );
    FF_getMajorBlockNumber_ExpectAndReturn( &xIOManager, 0U, 1U, 0U );
    FF_getMinorBlockNumber_ExpectAndReturn( &xIOManager, 0U, 1U, 0U );
    FF_LockFAT_Expect( &xIOManager );
    FF_getFATEntry_ExpectAnyArgsAndReturn( TEST_DATA_CLUSTER + 1U );
    FF_UnlockFAT_Expect( &xIOManager );
    FF_ReleaseFATBuffers_ExpectAnyArgsAndReturn( FF_ERR_NONE );
    FF_Cluster2LBA_ExpectAndReturn( &xIOManager, TEST_DATA_CLUSTER, TEST_DATA_LBA );
    FF_BlockWrite_ExpectAndReturn( &xIOManager,
                                   TEST_DATA_LBA,
                                   TEST_SECTORS_PER_CLUSTER * 2U,
                                   pucData,
                                   pdFALSE,
                                   FF_ERR_NONE );
    FF_LockFAT_Expect( &xIOManager );
    FF_TraverseFAT_ExpectAndReturn( &xIOManager,
                                    TEST_DATA_CLUSTER,
                                    2U,
                                    NULL,
                                    TEST_DATA_CLUSTER + 2U );
    FF_TraverseFAT_IgnoreArg_pxError();
    FF_TraverseFAT_ReturnThruPtr_pxError( &xTraverseError );
    FF_UnlockFAT_Expect( &xIOManager );

    TEST_ASSERT_EQUAL( xTraverseError, FF_Write( &xFile, 1U, sizeof( pucData ), pucData ) );
    TEST_ASSERT_BITS_HIGH( FF_VALID_FLAG_MODIFIED, xFile.ulValidFlags );
}

void test_FF_Close_updates_modified_time_for_written_file_when_size_is_unchanged( void )
{
    FF_FILE * pxFile = malloc( sizeof( *pxFile ) );
    FF_DirEnt_t xEntry;
    FF_DirEnt_t xExpectedEntry;
    FF_SystemTime_t xUpdatedTime = { 0 };

    TEST_ASSERT_NOT_NULL( pxFile );
    prvInitFile( pxFile );
    pxFile->ulValidFlags = FF_VALID_FLAG_MODIFIED;

    memset( &xEntry, 0, sizeof( xEntry ) );
    xEntry.ulFileSize = TEST_FILE_SIZE;
    xEntry.xModifiedTime.Year = 2001U;

    xExpectedEntry = xEntry;
    xUpdatedTime.Year = TEST_UPDATED_YEAR;
    xUpdatedTime.Month = 7U;
    xUpdatedTime.Day = 1U;
    xUpdatedTime.Hour = 12U;
    xExpectedEntry.xModifiedTime = xUpdatedTime;

    prvExpectValidHandleCheck();
    FF_GetEntry_ExpectAndReturn( &xIOManager,
                                 TEST_DIR_ENTRY,
                                 TEST_DIR_CLUSTER,
                                 NULL,
                                 FF_ERR_NONE );
    FF_GetEntry_IgnoreArg_pxDirent();
    FF_GetEntry_ReturnThruPtr_pxDirent( &xEntry );
    FF_GetSystemTime_ExpectAndReturn( NULL, 0 );
    FF_GetSystemTime_IgnoreArg_pxTime();
    FF_GetSystemTime_ReturnThruPtr_pxTime( &xUpdatedTime );
    FF_PutEntry_ExpectWithArrayAndReturn( &xIOManager,
                                          1,
                                          TEST_DIR_ENTRY,
                                          TEST_DIR_CLUSTER,
                                          &xExpectedEntry,
                                          1,
                                          NULL,
                                          0,
                                          FF_ERR_NONE );
    FF_PutEntry_IgnoreArg_pxIOManager();
    FF_PutEntry_IgnoreArg_pucContents();
    prvExpectCloseListRemoval();
    FF_FlushCache_ExpectAndReturn( &xIOManager, FF_ERR_NONE );

    TEST_ASSERT_EQUAL( FF_ERR_NONE, FF_Close( pxFile ) );
}

void test_FF_Close_does_not_write_directory_entry_when_file_is_not_modified( void )
{
    FF_FILE * pxFile = malloc( sizeof( *pxFile ) );
    FF_DirEnt_t xEntry;

    TEST_ASSERT_NOT_NULL( pxFile );
    prvInitFile( pxFile );

    memset( &xEntry, 0, sizeof( xEntry ) );
    xEntry.ulFileSize = TEST_FILE_SIZE;

    prvExpectValidHandleCheck();
    FF_GetEntry_ExpectAndReturn( &xIOManager,
                                 TEST_DIR_ENTRY,
                                 TEST_DIR_CLUSTER,
                                 NULL,
                                 FF_ERR_NONE );
    FF_GetEntry_IgnoreArg_pxDirent();
    FF_GetEntry_ReturnThruPtr_pxDirent( &xEntry );
    prvExpectCloseListRemoval();
    FF_FlushCache_ExpectAndReturn( &xIOManager, FF_ERR_NONE );

    TEST_ASSERT_EQUAL( FF_ERR_NONE, FF_Close( pxFile ) );
}
