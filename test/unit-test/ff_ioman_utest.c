/*
 * Unit tests for partition enumeration in ff_ioman.c.
 *
 * SPDX-License-Identifier: MIT
 *
 * These tests focus on the extended/logical partition (EBR chain) walk in
 * FF_ParseExtended(), reached through the public FF_PartitionSearch() API.
 * They specifically cover the bounds checks that prevent writing past
 * FF_SPartFound_t::pxPartitions[ ffconfigMAX_PARTITIONS ] when a disk presents
 * more logical partitions than the build is configured to hold, including a
 * self-referential EBR chain that would otherwise loop forever.
 *
 * The module under test is linked against CMock generated mocks of its
 * locking / FAT / CRC dependencies; the FreeRTOS+FAT byte accessors and buffer
 * management run for real, fed by an in-memory block device.
 */

#include <stdint.h>
#include <string.h>

#include "unity.h"

/* CMock generated mocks for the dependencies of ff_ioman.c. */
#include "mock_ff_locking.h"
#include "mock_ff_fat.h"
#include "mock_ff_crc.h"

/* The module under test (public API) and its types. */
#include "ff_headers.h"

/*-----------------------------------------------------------*/
/* Virtual disk + block device callback.                      */
/*-----------------------------------------------------------*/

#define TEST_SECTOR_SIZE      ( 512U )
#define TEST_DISK_SECTORS     ( 256U )
#define TEST_CACHE_SECTORS    ( 8U )

/* Partition-table byte offsets within an MBR/EBR entry. */
#define PTBL_BASE             ( 0x1BEU )
#define PTBL_ENTRY_SIZE       ( 16U )

static uint8_t ucVirtualDisk[ TEST_DISK_SECTORS * TEST_SECTOR_SIZE ];

static int32_t prvReadBlocks( uint8_t * pucBuffer,
                              uint32_t ulSectorAddress,
                              uint32_t ulCount,
                              FF_Disk_t * pxDisk )
{
    ( void ) pxDisk;

    if( ( ulSectorAddress + ulCount ) > TEST_DISK_SECTORS )
    {
        return -1;
    }

    memcpy( pucBuffer,
            &ucVirtualDisk[ ulSectorAddress * TEST_SECTOR_SIZE ],
            ulCount * TEST_SECTOR_SIZE );

    return ( int32_t ) ulCount;
}

static int32_t prvWriteBlocks( uint8_t * pucBuffer,
                               uint32_t ulSectorAddress,
                               uint32_t ulCount,
                               FF_Disk_t * pxDisk )
{
    ( void ) pucBuffer;
    ( void ) ulSectorAddress;
    ( void ) pxDisk;

    /* The partition-search path is read-only. */
    return ( int32_t ) ulCount;
}

/*-----------------------------------------------------------*/
/* Helpers to lay out MBR/EBR sectors.                        */
/*-----------------------------------------------------------*/

static void prvPutLong( uint8_t * pucSector,
                        uint32_t ulOffset,
                        uint32_t ulValue )
{
    pucSector[ ulOffset + 0 ] = ( uint8_t ) ( ulValue & 0xFFU );
    pucSector[ ulOffset + 1 ] = ( uint8_t ) ( ( ulValue >> 8 ) & 0xFFU );
    pucSector[ ulOffset + 2 ] = ( uint8_t ) ( ( ulValue >> 16 ) & 0xFFU );
    pucSector[ ulOffset + 3 ] = ( uint8_t ) ( ( ulValue >> 24 ) & 0xFFU );
}

static void prvSetSignature( uint8_t * pucSector )
{
    pucSector[ 0x1FE ] = 0x55;
    pucSector[ 0x1FF ] = 0xAA;
}

/* Write one partition-table entry into a 512-byte sector. */
static void prvSetPartEntry( uint8_t * pucSector,
                             uint32_t ulEntryIdx,
                             uint8_t ucActive,
                             uint8_t ucPartitionID,
                             uint32_t ulStartLBA,
                             uint32_t ulSectorCount )
{
    uint32_t ulOffset = PTBL_BASE + ( ulEntryIdx * PTBL_ENTRY_SIZE );

    pucSector[ ulOffset + FF_FAT_PTBL_ACTIVE ] = ucActive;
    pucSector[ ulOffset + FF_FAT_PTBL_ID ] = ucPartitionID;
    prvPutLong( pucSector, ulOffset + FF_FAT_PTBL_LBA, ulStartLBA );
    prvPutLong( pucSector, ulOffset + FF_FAT_PTBL_SECT_COUNT, ulSectorCount );
}

static uint8_t * prvSector( uint32_t ulSectorNr )
{
    return &ucVirtualDisk[ ulSectorNr * TEST_SECTOR_SIZE ];
}

/*-----------------------------------------------------------*/
/* I/O manager lifecycle for a single test.                   */
/*-----------------------------------------------------------*/

static FF_IOManager_t * prvCreateTestIOManager( void )
{
    FF_CreationParameters_t xParameters;
    FF_Error_t xError = FF_ERR_NONE;

    memset( &xParameters, 0, sizeof( xParameters ) );
    xParameters.ulMemorySize = TEST_CACHE_SECTORS * TEST_SECTOR_SIZE;
    xParameters.ulSectorSize = TEST_SECTOR_SIZE;
    xParameters.fnReadBlocks = prvReadBlocks;
    xParameters.fnWriteBlocks = prvWriteBlocks;
    xParameters.pxDisk = NULL;
    xParameters.pvSemaphore = NULL;
    xParameters.xBlockDeviceIsReentrant = pdTRUE;

    return FF_CreateIOManager( &xParameters, &xError );
}

/*
 * Build an MBR (sector 0) whose first entry is an extended partition, and a
 * forward-linked EBR chain. Each EBR defines one logical (data) partition and,
 * unless it is the last link, points to the next EBR.
 *
 * The extended link is placed in entry 0 (before the data partition in entry 1)
 * on purpose: this is the adversarial ordering in which the next-extended
 * pointer is discovered before the data partition is stored, so the count limit
 * must be enforced before the store rather than after it.
 */
static void prvBuildExtendedChain( uint32_t ulFirstSector,
                                   uint32_t ulEbrCount,
                                   uint32_t ulEbrSpacing )
{
    uint32_t i;
    const uint32_t ulExtendedSize = 4096U; /* generous, passes sanity checks */

    memset( ucVirtualDisk, 0, sizeof( ucVirtualDisk ) );

    /* MBR: a single extended partition that contains the whole chain. */
    prvSetPartEntry( prvSector( 0 ), 0, 0x00, FF_DOS_EXT_PART,
                     ulFirstSector, ulExtendedSize );
    prvSetSignature( prvSector( 0 ) );

    for( i = 0; i < ulEbrCount; i++ )
    {
        uint32_t ulThisEbr = ulFirstSector + ( i * ulEbrSpacing );
        uint8_t * pucEbr = prvSector( ulThisEbr );

        /* Entry 0: link to the next EBR (relative to the first EBR), unless
         * this is the final link in the chain. */
        if( i < ( ulEbrCount - 1 ) )
        {
            uint32_t ulNextRel = ( i + 1 ) * ulEbrSpacing;
            prvSetPartEntry( pucEbr, 0, 0x00, FF_DOS_EXT_PART,
                             ulNextRel, ulExtendedSize );
        }

        /* Entry 1: a logical data partition, located 1 sector past the EBR. */
        prvSetPartEntry( pucEbr, 1, 0x00, FF_T_FAT32 /* 0x0C */, 1U, 8U );

        prvSetSignature( pucEbr );
    }
}

/*
 * Build an MBR plus a single EBR that links to itself - a malformed,
 * self-referential chain. Each visit yields one logical partition, so without
 * a count cap the parser would loop forever and overflow the array.
 */
static void prvBuildSelfReferentialChain( uint32_t ulFirstSector )
{
    uint8_t * pucEbr;
    const uint32_t ulExtendedSize = 4096U;

    memset( ucVirtualDisk, 0, sizeof( ucVirtualDisk ) );

    prvSetPartEntry( prvSector( 0 ), 0, 0x00, FF_DOS_EXT_PART,
                     ulFirstSector, ulExtendedSize );
    prvSetSignature( prvSector( 0 ) );

    pucEbr = prvSector( ulFirstSector );

    /* Entry 0: extended link with relative LBA 0 -> resolves back to
     * ulFirstSector, i.e. the EBR points at itself. */
    prvSetPartEntry( pucEbr, 0, 0x00, FF_DOS_EXT_PART, 0U, ulExtendedSize );
    /* Entry 1: logical data partition, yielded on every visit. */
    prvSetPartEntry( pucEbr, 1, 0x00, FF_T_FAT32, 1U, 8U );
    prvSetSignature( pucEbr );
}

/*-----------------------------------------------------------*/
/* A guarded FF_SPartFound_t to detect writes past the array. */
/*-----------------------------------------------------------*/

#define GUARD_BYTES      ( 64U )
#define GUARD_PATTERN    ( 0xA5U )

typedef struct
{
    FF_SPartFound_t xFound;
    uint8_t ucGuard[ GUARD_BYTES ];
} GuardedPartsFound_t;

static int prvGuardIntact( const GuardedPartsFound_t * pxGuarded )
{
    uint32_t i;

    for( i = 0; i < GUARD_BYTES; i++ )
    {
        if( pxGuarded->ucGuard[ i ] != GUARD_PATTERN )
        {
            return 0;
        }
    }

    return 1;
}

/*-----------------------------------------------------------*/
/* Unity fixtures.                                            */
/*-----------------------------------------------------------*/

void setUp( void )
{
    memset( ucVirtualDisk, 0, sizeof( ucVirtualDisk ) );

    /* The locking layer is irrelevant to partition parsing: let every call
     * pass through. FF_CreateEvents must report success so the I/O manager is
     * created. */
    FF_CreateEvents_IgnoreAndReturn( pdTRUE );
    FF_DeleteEvents_Ignore();
    FF_PendSemaphore_Ignore();
    FF_ReleaseSemaphore_Ignore();
    FF_BufferWait_IgnoreAndReturn( pdTRUE );
    FF_BufferProceed_Ignore();
    FF_Sleep_Ignore();
}

void tearDown( void )
{
}

/*-----------------------------------------------------------*/
/* Tests.                                                     */
/*-----------------------------------------------------------*/

/*
 * More logical partitions on disk than ffconfigMAX_PARTITIONS: the result must
 * be clamped and the array must not be overrun.
 */
void test_PartitionSearch_overflowing_chain_is_clamped( void )
{
    FF_IOManager_t * pxIOManager;
    GuardedPartsFound_t xGuarded;
    FF_Error_t xResult;

    memset( &xGuarded, 0, sizeof( xGuarded ) );
    memset( xGuarded.ucGuard, GUARD_PATTERN, GUARD_BYTES );

    /* 6 logical partitions, but ffconfigMAX_PARTITIONS is 4. */
    prvBuildExtendedChain( 100U, 6U, 10U );

    pxIOManager = prvCreateTestIOManager();
    TEST_ASSERT_NOT_NULL( pxIOManager );

    xResult = FF_PartitionSearch( pxIOManager, &xGuarded.xFound );

    TEST_ASSERT_FALSE( FF_isERR( xResult ) );
    TEST_ASSERT_EQUAL_INT( ffconfigMAX_PARTITIONS, xGuarded.xFound.iCount );
    TEST_ASSERT_TRUE_MESSAGE( prvGuardIntact( &xGuarded ),
                              "partition array was written out of bounds" );

    ( void ) FF_DeleteIOManager( pxIOManager );
}

/*
 * Exactly ffconfigMAX_PARTITIONS logical partitions: all are recorded and the
 * guard remains intact.
 */
void test_PartitionSearch_chain_at_limit_is_recorded( void )
{
    FF_IOManager_t * pxIOManager;
    GuardedPartsFound_t xGuarded;
    FF_Error_t xResult;

    memset( &xGuarded, 0, sizeof( xGuarded ) );
    memset( xGuarded.ucGuard, GUARD_PATTERN, GUARD_BYTES );

    prvBuildExtendedChain( 100U, ( uint32_t ) ffconfigMAX_PARTITIONS, 10U );

    pxIOManager = prvCreateTestIOManager();
    TEST_ASSERT_NOT_NULL( pxIOManager );

    xResult = FF_PartitionSearch( pxIOManager, &xGuarded.xFound );

    TEST_ASSERT_FALSE( FF_isERR( xResult ) );
    TEST_ASSERT_EQUAL_INT( ffconfigMAX_PARTITIONS, xGuarded.xFound.iCount );
    TEST_ASSERT_TRUE_MESSAGE( prvGuardIntact( &xGuarded ),
                              "partition array was written out of bounds" );

    ( void ) FF_DeleteIOManager( pxIOManager );
}

/*
 * A self-referential EBR chain would loop forever and overrun the array if the
 * count were not capped. The search must terminate, clamp the count, and leave
 * the guard intact.
 */
void test_PartitionSearch_self_referential_chain_terminates( void )
{
    FF_IOManager_t * pxIOManager;
    GuardedPartsFound_t xGuarded;
    FF_Error_t xResult;

    memset( &xGuarded, 0, sizeof( xGuarded ) );
    memset( xGuarded.ucGuard, GUARD_PATTERN, GUARD_BYTES );

    prvBuildSelfReferentialChain( 200U );

    pxIOManager = prvCreateTestIOManager();
    TEST_ASSERT_NOT_NULL( pxIOManager );

    xResult = FF_PartitionSearch( pxIOManager, &xGuarded.xFound );

    TEST_ASSERT_FALSE( FF_isERR( xResult ) );
    TEST_ASSERT_EQUAL_INT( ffconfigMAX_PARTITIONS, xGuarded.xFound.iCount );
    TEST_ASSERT_TRUE_MESSAGE( prvGuardIntact( &xGuarded ),
                              "partition array was written out of bounds" );

    ( void ) FF_DeleteIOManager( pxIOManager );
}
