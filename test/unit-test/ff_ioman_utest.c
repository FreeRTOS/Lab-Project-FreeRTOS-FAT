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

/*-----------------------------------------------------------*/
/* GPT header validation in FF_GetEfiPartitionEntry().        */
/*                                                            */
/* FF_GetEfiPartitionEntry() is static; it is reached through */
/* the public FF_Mount() API when the MBR advertises a GPT    */
/* protective partition (type 0xEE). The 'HeaderSize' field   */
/* (offset 0x0C) is disk-controlled and is used as the byte   */
/* count for FF_GetCRC32() over the one-sector header buffer, */
/* so it must be bounded to [92, usSectorSize] before use.    */
/*                                                            */
/* FF_GetCRC32() is a mock: in the rejection tests it is left */
/* without any expectation, so CMock fails the test if the    */
/* out-of-bounds CRC call is ever reached.                    */
/*-----------------------------------------------------------*/

/* GPT byte offsets within the header sector (mirror of the private
 * definitions in ff_ioman.c). */
#define GPT_OFF_HEAD_LENGTH            ( 0x0CU )
#define GPT_OFF_HEAD_CRC               ( 0x10U )
#define GPT_OFF_HEAD_PART_ENTRY_LBA    ( 0x48U )
#define GPT_OFF_HEAD_ENTRY_SIZE        ( 0x54U )

#define GPT_PROTECTIVE_PART_ID         ( 0xEEU )
#define GPT_HEADER_SECTOR              ( 1U )

/* UEFI-mandated minimum GPT header size, mirrors FF_GPT_HEAD_MIN_LENGTH in
 * ff_ioman.c. */
#define GPT_HEAD_MIN_LENGTH            ( 92U )

static FF_Disk_t xTestDisk;

/*
 * Lay out an MBR (sector 0) that advertises a single GPT protective partition
 * pointing at a GPT header sector, and write a GPT header there with a caller
 * supplied 'HeaderSize' and header CRC.
 */
static void prvBuildGptDisk( uint32_t ulHeaderLength,
                             uint32_t ulHeaderCrc )
{
    uint8_t * pucMbr;
    uint8_t * pucHeader;

    memset( ucVirtualDisk, 0, sizeof( ucVirtualDisk ) );

    /* MBR: one non-extended protective partition (type 0xEE). */
    pucMbr = prvSector( 0 );
    prvSetPartEntry( pucMbr, 0, 0x00, GPT_PROTECTIVE_PART_ID,
                     GPT_HEADER_SECTOR, TEST_DISK_SECTORS - GPT_HEADER_SECTOR );
    prvSetSignature( pucMbr );

    /* GPT header sector. */
    pucHeader = prvSector( GPT_HEADER_SECTOR );
    memcpy( pucHeader, "EFI PART", 8 );
    prvPutLong( pucHeader, GPT_OFF_HEAD_LENGTH, ulHeaderLength );
    prvPutLong( pucHeader, GPT_OFF_HEAD_CRC, ulHeaderCrc );
    /* Only consulted if the header passes validation. */
    prvPutLong( pucHeader, GPT_OFF_HEAD_ENTRY_SIZE, 128U );
    prvPutLong( pucHeader, GPT_OFF_HEAD_PART_ENTRY_LBA, GPT_HEADER_SECTOR + 1U );
}

static FF_IOManager_t * prvCreateMountableDisk( void )
{
    FF_IOManager_t * pxIOManager = prvCreateTestIOManager();

    memset( &xTestDisk, 0, sizeof( xTestDisk ) );
    xTestDisk.pxIOManager = pxIOManager;

    return pxIOManager;
}

/*
 * A 'HeaderSize' far larger than the sector buffer (0xFFFFFFFF, the value the
 * PoC sets) must be rejected as an invalid format, and the out-of-bounds
 * FF_GetCRC32() over the header buffer must never be reached.
 */
void test_Mount_gpt_header_length_oversized_is_rejected( void )
{
    FF_IOManager_t * pxIOManager;
    FF_Error_t xError;

    prvBuildGptDisk( 0xFFFFFFFFU, 0x12345678U );

    pxIOManager = prvCreateMountableDisk();
    TEST_ASSERT_NOT_NULL( pxIOManager );

    xError = FF_Mount( &xTestDisk, 0 );

    TEST_ASSERT_TRUE( FF_isERR( xError ) );
    TEST_ASSERT_EQUAL_HEX32( FF_ERR_IOMAN_INVALID_FORMAT, FF_GETERROR( xError ) );

    ( void ) FF_DeleteIOManager( pxIOManager );
}

/*
 * Boundary: 'HeaderSize' one byte past the sector size must be rejected and
 * must not reach the CRC.
 */
void test_Mount_gpt_header_length_one_past_sector_is_rejected( void )
{
    FF_IOManager_t * pxIOManager;
    FF_Error_t xError;

    prvBuildGptDisk( TEST_SECTOR_SIZE + 1U, 0x12345678U );

    pxIOManager = prvCreateMountableDisk();
    TEST_ASSERT_NOT_NULL( pxIOManager );

    xError = FF_Mount( &xTestDisk, 0 );

    TEST_ASSERT_TRUE( FF_isERR( xError ) );
    TEST_ASSERT_EQUAL_HEX32( FF_ERR_IOMAN_INVALID_FORMAT, FF_GETERROR( xError ) );

    ( void ) FF_DeleteIOManager( pxIOManager );
}

/*
 * Boundary: 'HeaderSize' below the UEFI minimum (92 bytes) must be rejected and
 * must not reach the CRC.
 */
void test_Mount_gpt_header_length_below_minimum_is_rejected( void )
{
    FF_IOManager_t * pxIOManager;
    FF_Error_t xError;

    prvBuildGptDisk( 91U, 0x12345678U );

    pxIOManager = prvCreateMountableDisk();
    TEST_ASSERT_NOT_NULL( pxIOManager );

    xError = FF_Mount( &xTestDisk, 0 );

    TEST_ASSERT_TRUE( FF_isERR( xError ) );
    TEST_ASSERT_EQUAL_HEX32( FF_ERR_IOMAN_INVALID_FORMAT, FF_GETERROR( xError ) );

    ( void ) FF_DeleteIOManager( pxIOManager );
}

/*
 * A 'HeaderSize' at the UEFI minimum (92 bytes) is in range, so the bounds
 * check passes and the CRC is computed. With a mismatching (mocked) CRC the
 * header is reported corrupt - proving the valid-length branch proceeds to the
 * CRC instead of being rejected.
 */
void test_Mount_gpt_header_length_at_minimum_reaches_crc( void )
{
    FF_IOManager_t * pxIOManager;
    FF_Error_t xError;

    prvBuildGptDisk( GPT_HEAD_MIN_LENGTH, 0x12345678U );

    /* In-range length: the CRC routine is reached exactly once. The mocked
     * result differs from the stored header CRC, so the header is "corrupt". */
    FF_GetCRC32_ExpectAnyArgsAndReturn( 0xDEADBEEFU );

    pxIOManager = prvCreateMountableDisk();
    TEST_ASSERT_NOT_NULL( pxIOManager );

    xError = FF_Mount( &xTestDisk, 0 );

    TEST_ASSERT_TRUE( FF_isERR( xError ) );
    TEST_ASSERT_EQUAL_HEX32( FF_ERR_IOMAN_GPT_HEADER_CORRUPT, FF_GETERROR( xError ) );

    ( void ) FF_DeleteIOManager( pxIOManager );
}

/*
 * Boundary: 'HeaderSize' exactly equal to the sector size is the largest
 * in-range value, so it must also reach the CRC rather than be rejected.
 */
void test_Mount_gpt_header_length_at_sector_size_reaches_crc( void )
{
    FF_IOManager_t * pxIOManager;
    FF_Error_t xError;

    prvBuildGptDisk( TEST_SECTOR_SIZE, 0x12345678U );

    FF_GetCRC32_ExpectAnyArgsAndReturn( 0xDEADBEEFU );

    pxIOManager = prvCreateMountableDisk();
    TEST_ASSERT_NOT_NULL( pxIOManager );

    xError = FF_Mount( &xTestDisk, 0 );

    TEST_ASSERT_TRUE( FF_isERR( xError ) );
    TEST_ASSERT_EQUAL_HEX32( FF_ERR_IOMAN_GPT_HEADER_CORRUPT, FF_GETERROR( xError ) );

    ( void ) FF_DeleteIOManager( pxIOManager );
}

/*-----------------------------------------------------------*/
/* Sector-size validation in FF_CreateIOManager().            */
/*-----------------------------------------------------------*/

/*
 * A sector size that does not fit in the uint16_t 'usSectorSize' field (e.g.
 * 65536, which is a multiple of 512 and so passes the old check) would
 * truncate to 0. It must instead fail cleanly with FF_ERR_IOMAN_BAD_BLKSIZE
 * and return NULL.
 */
void test_CreateIOManager_rejects_sector_size_above_uint16( void )
{
    FF_CreationParameters_t xParameters;
    FF_Error_t xError = FF_ERR_NONE;
    FF_IOManager_t * pxIOManager;

    memset( &xParameters, 0, sizeof( xParameters ) );
    xParameters.ulSectorSize = 0x10000;      /* 65536 -> 0 when truncated. */
    xParameters.ulMemorySize = 2U * 0x10000; /* Two sectors of cache.      */
    xParameters.fnReadBlocks = prvReadBlocks;
    xParameters.fnWriteBlocks = prvWriteBlocks;
    xParameters.xBlockDeviceIsReentrant = pdTRUE;

    pxIOManager = FF_CreateIOManager( &xParameters, &xError );

    TEST_ASSERT_NULL( pxIOManager );
    TEST_ASSERT_TRUE( FF_isERR( xError ) );
    TEST_ASSERT_EQUAL_HEX32( FF_ERR_IOMAN_BAD_BLKSIZE, FF_GETERROR( xError ) );
}

/*
 * Sector sizes larger than 512 but within the uint16_t range (e.g. 4096) remain
 * valid: the upper-bound check must not reject legitimate large sectors.
 */
void test_CreateIOManager_accepts_4096_byte_sector( void )
{
    FF_CreationParameters_t xParameters;
    FF_Error_t xError = FF_ERR_NONE;
    FF_IOManager_t * pxIOManager;

    memset( &xParameters, 0, sizeof( xParameters ) );
    xParameters.ulSectorSize = 4096;
    xParameters.ulMemorySize = 8U * 4096U;
    xParameters.fnReadBlocks = prvReadBlocks;
    xParameters.fnWriteBlocks = prvWriteBlocks;
    xParameters.xBlockDeviceIsReentrant = pdTRUE;

    pxIOManager = FF_CreateIOManager( &xParameters, &xError );

    TEST_ASSERT_NOT_NULL( pxIOManager );
    TEST_ASSERT_FALSE( FF_isERR( xError ) );
    TEST_ASSERT_EQUAL_UINT16( 4096U, pxIOManager->usSectorSize );

    ( void ) FF_DeleteIOManager( pxIOManager );
}
