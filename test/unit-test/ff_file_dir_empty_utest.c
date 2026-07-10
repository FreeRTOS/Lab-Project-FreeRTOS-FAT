/*
 * Unit tests for directory-empty checks in ff_file.c.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "mock_ff_dir_is_empty_mock_subset.h"

#include "ff_headers.h"

#define TEST_ROOT_DIR_CLUSTER    ( 1U )
#define TEST_SUBDIR_CLUSTER      ( 7U )

static FF_IOManager_t xIOManager;

static BaseType_t prvCheckEmptyDirectory( const char * pcPath,
                                          uint32_t ulDirCluster )
{
    FF_Error_t xFindError = FF_ERR_NONE;

    FF_FindDir_ExpectAndReturn( &xIOManager,
                                pcPath,
                                ( uint16_t ) strlen( pcPath ),
                                NULL,
                                ulDirCluster );
    FF_FindDir_IgnoreArg_pxError();
    FF_FindDir_ReturnThruPtr_pxError( &xFindError );
    FF_InitEntryFetch_ExpectAndReturn( &xIOManager,
                                       ulDirCluster,
                                       NULL,
                                       FF_ERR_NONE );
    FF_InitEntryFetch_IgnoreArg_pxContext();
    FF_FindNext_ExpectAndReturn( &xIOManager,
                                 NULL,
                                 FF_createERR( FF_ERR_DIR_END_OF_DIR, FF_FINDNEXT ) );
    FF_FindNext_IgnoreArg_pxDirEntry();

    return FF_isDirEmpty( &xIOManager, pcPath );
}

void setUp( void )
{
    memset( &xIOManager, 0, sizeof( xIOManager ) );
}

void tearDown( void )
{
}

void test_FF_isDirEmpty_accepts_FAT12_root_directory_cluster( void )
{
    xIOManager.xPartition.ucType = FF_T_FAT12;
    xIOManager.xPartition.ulRootDirCluster = TEST_ROOT_DIR_CLUSTER;

    TEST_ASSERT_EQUAL( pdTRUE,
                       prvCheckEmptyDirectory( "/", TEST_ROOT_DIR_CLUSTER ) );
}

void test_FF_isDirEmpty_accepts_FAT16_root_directory_cluster( void )
{
    xIOManager.xPartition.ucType = FF_T_FAT16;
    xIOManager.xPartition.ulRootDirCluster = TEST_ROOT_DIR_CLUSTER;

    TEST_ASSERT_EQUAL( pdTRUE,
                       prvCheckEmptyDirectory( "/", TEST_ROOT_DIR_CLUSTER ) );
}

void test_FF_isDirEmpty_handles_directory_with_or_without_trailing_separator( void )
{
    TEST_ASSERT_EQUAL( pdTRUE,
                       prvCheckEmptyDirectory( "/dir", TEST_SUBDIR_CLUSTER ) );
    TEST_ASSERT_EQUAL( pdTRUE,
                       prvCheckEmptyDirectory( "/dir/", TEST_SUBDIR_CLUSTER ) );
}

void test_FF_isDirEmpty_reports_directory_entry_as_non_empty( void )
{
    FF_DirEnt_t xDirEntry;
    FF_Error_t xFindError = FF_ERR_NONE;
    const char * pcPath = "/dir";

    memset( &xDirEntry, 0, sizeof( xDirEntry ) );
    ( void ) strcpy( xDirEntry.pcFileName, "file.txt" );

    FF_FindDir_ExpectAndReturn( &xIOManager,
                                pcPath,
                                ( uint16_t ) strlen( pcPath ),
                                NULL,
                                TEST_SUBDIR_CLUSTER );
    FF_FindDir_IgnoreArg_pxError();
    FF_FindDir_ReturnThruPtr_pxError( &xFindError );
    FF_InitEntryFetch_ExpectAndReturn( &xIOManager,
                                       TEST_SUBDIR_CLUSTER,
                                       NULL,
                                       FF_ERR_NONE );
    FF_InitEntryFetch_IgnoreArg_pxContext();
    FF_FindNext_ExpectAndReturn( &xIOManager, NULL, FF_ERR_NONE );
    FF_FindNext_IgnoreArg_pxDirEntry();
    FF_FindNext_ReturnThruPtr_pxDirEntry( &xDirEntry );

    TEST_ASSERT_EQUAL( pdFALSE, FF_isDirEmpty( &xIOManager, pcPath ) );
}
