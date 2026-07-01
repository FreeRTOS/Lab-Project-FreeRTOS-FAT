/*
 * FreeRTOS+FAT unit-test configuration.
 *
 * SPDX-License-Identifier: MIT
 *
 * Minimal configuration used to host-compile the FreeRTOS+FAT sources for
 * unit testing. ffconfigMAX_PARTITIONS is deliberately kept small so the
 * partition-enumeration bounds checks in ff_ioman.c can be exercised with a
 * modest, easy to read on-disk layout.
 */

#ifndef FREERTOS_FAT_CONFIG_H
#define FREERTOS_FAT_CONFIG_H

#include <stdlib.h>

/* The host (and CI runners) are little endian. */
#define ffconfigBYTE_ORDER                ( pdFREERTOS_LITTLE_ENDIAN )

#define ffconfigCWD_THREAD_LOCAL_INDEX    ( 0 )

#if !defined( portINLINE )
    #define portINLINE                    __inline
#endif

/* Keep the maximum partition count small so the EBR-chain bounds checks in
 * FF_ParseExtended()/FF_PartitionSearch() are reachable with a compact disk
 * image. Must remain within the 1..8 range enforced by the defaults header. */
#define ffconfigMAX_PARTITIONS    ( 4 )

/* Use the standard C allocator on the host so the I/O manager can be created
 * without a running FreeRTOS heap. */
#define ffconfigMALLOC( size )    malloc( size )
#define ffconfigFREE( ptr )       free( ptr )

/* CMock does not evaluate preprocessor conditionals, so for the dual-prototype
 * helpers (FF_GetFreeSize / FF_GetVolumeSize) it generates the 64-bit variant.
 * Select the matching 64-bit configuration here so the real declarations agree
 * with the generated mocks. */
#define ffconfig64_NUM_SUPPORT    ( 1 )

#define ffconfigTIME_SUPPORT                         ( 1 )
#define ffconfigUPDATE_FILE_MODIFIED_TIME_ON_CLOSE    ( 1 )

/* All other ffconfig values fall back to FreeRTOSFATConfigDefaults.h. */

#endif /* FREERTOS_FAT_CONFIG_H */
