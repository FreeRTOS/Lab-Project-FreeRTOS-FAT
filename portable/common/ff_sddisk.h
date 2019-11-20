/*
 * FreeRTOS+FAT Labs Build 160919a (C) 2016 Real Time Engineers ltd.
 * Authors include James Walmsley, Hein Tibosch and Richard Barry
 *
 *******************************************************************************
 ***** NOTE ******* NOTE ******* NOTE ******* NOTE ******* NOTE ******* NOTE ***
 ***                                                                         ***
 ***                                                                         ***
 ***   FREERTOS+FAT IS STILL IN THE LAB:                                     ***
 ***                                                                         ***
 ***   Be aware we are still refining the FreeRTOS+FAT design,               ***
 ***   the source code does not yet fully conform to the strict quality and  ***
 ***   style standards mandated by Real Time Engineers ltd., and the         ***
 ***   documentation and testing is not necessarily complete.                ***
 ***                                                                         ***
 ***   PLEASE REPORT EXPERIENCES USING THE SUPPORT RESOURCES FOUND ON THE    ***
 ***   URL: http://www.FreeRTOS.org/contact  Active early adopters may, at   ***
 ***   the sole discretion of Real Time Engineers Ltd., be offered versions  ***
 ***   under a license other than that described below.                      ***
 ***                                                                         ***
 ***                                                                         ***
 ***** NOTE ******* NOTE ******* NOTE ******* NOTE ******* NOTE ******* NOTE ***
 *******************************************************************************
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
 * 1 tab == 4 spaces!
 *
 * http://www.FreeRTOS.org
 * http://www.FreeRTOS.org/plus
 * http://www.FreeRTOS.org/labs
 *
 */

#ifndef __SDDISK_H__

#define __SDDISK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ff_headers.h"


/* Return non-zero if the SD-card is present.
The parameter 'pxDisk' may be null, unless device locking is necessary. */
BaseType_t FF_SDDiskDetect( FF_Disk_t *pxDisk );

/* Create a RAM disk, supplying enough memory to hold N sectors of 512 bytes each */
FF_Disk_t *FF_SDDiskInit( const char *pcName );

BaseType_t FF_SDDiskReinit( FF_Disk_t *pxDisk );

/* Unmount the volume */
BaseType_t FF_SDDiskUnmount( FF_Disk_t *pDisk );

/* Mount the volume */
BaseType_t FF_SDDiskMount( FF_Disk_t *pDisk );

/* Release all resources */
BaseType_t FF_SDDiskDelete( FF_Disk_t *pDisk );

/* Show some partition information */
BaseType_t FF_SDDiskShowPartition( FF_Disk_t *pDisk );

/* Flush changes from the driver's buf to disk */
void FF_SDDiskFlush( FF_Disk_t *pDisk );

/* Format a given partition on an SD-card. */
BaseType_t FF_SDDiskFormat( FF_Disk_t *pxDisk, BaseType_t aPart );

/* Return non-zero if an SD-card is detected in a given slot. */
BaseType_t FF_SDDiskInserted( BaseType_t xDriveNr );

/* _RB_ Temporary function - ideally the application would not need the IO
manageer structure, just a handle to a disk. */
FF_IOManager_t *sddisk_ioman( FF_Disk_t *pxDisk );

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __SDDISK_H__ */
