/*
 * Unit tests for file-close metadata updates in ff_file.c.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "mock_ff_locking.h"
#include "mock_ff_dir.h"
#include "mock_ff_ioman.h"
#include "mock_ff_time.h"

#include "ff_headers.h"

#define TEST_SECTOR_SIZE             ( 512U )
#define TEST_SECTORS_PER_CLUSTER     ( 4U )
#define TEST_DIR_ENTRY               ( 3U )
#define TEST_DIR_CLUSTER             ( 9U )
#define TEST_FILE_SIZE               ( 123U )
#define TEST_UPDATED_YEAR            ( 2026U )

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
