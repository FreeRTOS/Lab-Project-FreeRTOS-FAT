/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT
 */
#include "ff_sddisk.h"
#include "ff_sys.h"
#include "ff_socfpga.h"

int32_t FatReadAbsLayer( uint8_t *pucBuffer, uint32_t ulSectorNumber,
        uint32_t ulSectorCount, FF_Disk_t *pxDisk )
{
    int32_t ret = FF_ERR_IOMAN_OUT_OF_BOUNDS_READ | FF_ERRFLAG;
    if(pxDisk == NULL)
    {
        return -1;
    }
    if( pxDisk->ulDevAddr == -1 )
    {
        ret = FFReadSdmmc( pucBuffer, ulSectorNumber,ulSectorCount, pxDisk );
    }
    else if( pxDisk->ulDevAddr >= 0 )
    {
        ret = FFReadUsb( pucBuffer, ulSectorNumber,ulSectorCount, pxDisk );
    }
    else
    {
        return -1;
    }
    return ret;
}

int32_t FatWriteAbsLayer( uint8_t *pucBuffer, uint32_t ulSectorNumber,
        uint32_t ulSectorCount, FF_Disk_t *pxDisk )
{
    int32_t ret = FF_ERR_IOMAN_OUT_OF_BOUNDS_READ | FF_ERRFLAG;
    if(pxDisk == NULL)
    {
        return -1;
    }
    if( pxDisk->ulDevAddr == -1 )
    {
        ret = FFWriteSdmmc( pucBuffer, ulSectorNumber,ulSectorCount, pxDisk );
    }
    else if( pxDisk->ulDevAddr >= 0 )
    {
        ret = FFWriteUsb( pucBuffer, ulSectorNumber,ulSectorCount, pxDisk );
    }
    else
    {
        return -1;
    }
    return ret;
}
