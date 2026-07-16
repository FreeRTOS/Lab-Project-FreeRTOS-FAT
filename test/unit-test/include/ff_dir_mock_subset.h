#ifndef FF_DIR_MOCK_SUBSET_H
#define FF_DIR_MOCK_SUBSET_H

FF_Error_t FF_GetEntry( FF_IOManager_t * pxIOManager,
                        uint16_t usEntry,
                        uint32_t ulDirCluster,
                        FF_DirEnt_t * pxDirent );
FF_Error_t FF_PutEntry( FF_IOManager_t * pxIOManager,
                        uint16_t usEntry,
                        uint32_t ulDirCluster,
                        FF_DirEnt_t * pxDirent,
                        uint8_t * pucContents );

#endif /* FF_DIR_MOCK_SUBSET_H */
