#ifndef FF_IOMAN_MOCK_SUBSET_H
#define FF_IOMAN_MOCK_SUBSET_H

FF_Error_t FF_FlushCache( FF_IOManager_t * pxIOManager );
FF_Error_t FF_DecreaseFreeClusters( FF_IOManager_t * pxIOManager,
                                    uint32_t Count );
int32_t FF_BlockWrite( FF_IOManager_t * pxIOManager,
                       uint32_t ulSectorLBA,
                       uint32_t ulNumSectors,
                       void * pBuffer,
                       BaseType_t aSemLocked );
FF_Buffer_t * FF_GetBuffer( FF_IOManager_t * pxIOManager,
                            uint32_t ulSector,
                            uint8_t Mode );
FF_Error_t FF_ReleaseBuffer( FF_IOManager_t * pxIOManager,
                             FF_Buffer_t * pBuffer );

#endif /* FF_IOMAN_MOCK_SUBSET_H */
