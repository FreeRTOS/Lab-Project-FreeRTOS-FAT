/*
 * Minimal dependency subset for FF_isDirEmpty() unit tests.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef FF_DIR_IS_EMPTY_MOCK_SUBSET_H
#define FF_DIR_IS_EMPTY_MOCK_SUBSET_H

uint32_t FF_FindDir( FF_IOManager_t * pxIOManager,
                     const char * pcPath,
                     uint16_t usPathLength,
                     FF_Error_t * pxError );
FF_Error_t FF_InitEntryFetch( FF_IOManager_t * pxIOManager,
                              uint32_t ulDirCluster,
                              FF_FetchContext_t * pxContext );
FF_Error_t FF_FindNext( FF_IOManager_t * pxIOManager,
                        FF_DirEnt_t * pxDirEntry );

#endif /* FF_DIR_IS_EMPTY_MOCK_SUBSET_H */
