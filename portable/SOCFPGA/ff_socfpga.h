/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for xHCI implementation
 */

#ifndef __FF_SOCFPGA_H__
#define __FF_SOCFPGA_H__

#include <stdint.h>

int32_t FFWriteSdmmc( uint8_t *pucBuffer, uint32_t ulSectorNumber,
        uint32_t ulSectorCount, FF_Disk_t *pxDisk );

int32_t FFReadSdmmc( uint8_t *pucBuffer, uint32_t ulSectorNumber,
        uint32_t ulSectorCount, FF_Disk_t *pxDisk );

int32_t FatReadAbsLayer( uint8_t *pucBuffer, uint32_t ulSectorNumber,
        uint32_t ulSectorCount, FF_Disk_t *pxDisk );
int32_t FatWriteAbsLayer( uint8_t *pucBuffer, uint32_t ulSectorNumber,
        uint32_t ulSectorCount, FF_Disk_t *pxDisk );

#endif /* __FF_SOCFPGA_H__ */
