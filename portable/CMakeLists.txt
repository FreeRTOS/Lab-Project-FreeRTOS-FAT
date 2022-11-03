add_library( freertos_plus_fat_port_common STATIC )

target_sources( freertos_plus_fat_port_common
  PRIVATE
    common/ff_ramdisk.c
    common/ff_ramdisk.h
    common/ff_sddisk.h
)

target_include_directories( freertos_plus_fat_port_common SYSTEM
  PUBLIC
    common
)

target_compile_options( freertos_plus_fat_port_common
  PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-missing-prototypes>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-reserved-macro-identifier>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-shorten-64-to-32>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-sign-conversion>
)

target_link_libraries( freertos_plus_fat_port_common
  PRIVATE
    freertos_kernel
    freertos_plus_fat
)

# -------------------------------------------------------------------

if (FREERTOS_PLUS_FAT_PORT STREQUAL "A_CUSTOM_PORT")
    message(STATUS "Using a custom FREERTOS_PLUS_FAT_PORT.")
    return()
endif()

add_library( freertos_plus_fat_port STATIC )

target_sources( freertos_plus_fat_port
  PRIVATE
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},ATSAM4E>:
        ATSAM4E/ff_sddisk_r.c
        ATSAM4E/ff_sddisk.c>
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},AVR32_UC3>:
        avr32_uc3/ff_flush.c
        avr32_uc3/ff_flush.h
        avr32_uc3/ff_locking.c>
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},LPC18XX>:
        lpc18xx/ff_sddisk.c>
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},POSIX>:
        linux/ff_sddisk.c>
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},STM32F4XX>:
        STM32F4xx/ff_sddisk.c
        STM32F4xx/stm32f4xx_hal_sd.c
        STM32F4xx/stm32f4xx_hal_sd.h
        STM32F4xx/stm32f4xx_ll_sdmmc.c
        STM32F4xx/stm32f4xx_ll_sdmmc.h>
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},STM3F7XX>:
        STM32F7xx/ff_sddisk.c
        STM32F7xx/stm32f7xx_hal_sd.c
        STM32F7xx/stm32f7xx_hal_sd.h>
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},ZYNQ>:
        Zynq/ff_sddisk.c
        Zynq/xsdps_g.c
        Zynq/xsdps_hw.h
        Zynq/xsdps_info.c
        Zynq/xsdps_info.h
        Zynq/xsdps_options.c
        Zynq/xsdps_sinit.c
        Zynq/xsdps.c
        Zynq/xsdps.h>
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},ZYNQ_2019_3>:
        Zynq.2019.3/ff_sddisk.c
        Zynq.2019.3/xsdps_g.c
        Zynq.2019.3/xsdps_hw.h
        Zynq.2019.3/xsdps_info.c
        Zynq.2019.3/xsdps_info.h
        Zynq.2019.3/xsdps_options.c
        Zynq.2019.3/xsdps_sinit.c
        Zynq.2019.3/xsdps.c
        Zynq.2019.3/xsdps.h>
)

target_include_directories( freertos_plus_fat_port
  PRIVATE
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},AVR32_UC3>:${CMAKE_CURRENT_SOURCE_DIR}/avr32_uc3>
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},STM32F4XX>:${CMAKE_CURRENT_SOURCE_DIR}/STM32F4xx>
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},STM32F7XX>:${CMAKE_CURRENT_SOURCE_DIR}/STM32F7xx>
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},ZYNQ>:${CMAKE_CURRENT_SOURCE_DIR}/Zynq>
    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},ZYNQ_2019_3>:${CMAKE_CURRENT_SOURCE_DIR}/Zynq.2019.3>
)

target_compile_options( freertos_plus_fat_port
  PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-gnu-statement-expression>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-unused-parameter>
)

if( (FREERTOS_PLUS_FAT_PORT STREQUAL ZYNQ_2019_3) AND NOT TARGET freertos_xil_uncached_memory )
    # Note: the ZYNQ_2019_3 - assumes the use of freertos_xil_uncached_memory.h from F
    message(FATAL_ERROR "For FREERTOS_PLUS_FAT_PORT=ZYNQ_2019_3, must have a freertos_xil_uncached_memory target"
                   "This can be found in FreeRTOS-Plus-TCP NetworkInterface/Zynq/uncached_memory.*")
endif()

target_link_libraries( freertos_plus_fat_port
  PUBLIC
    freertos_plus_fat_port_common
  PRIVATE
    freertos_plus_fat
    freertos_kernel

    $<$<STREQUAL:${FREERTOS_PLUS_FAT_PORT},ZYNQ_2019_3>:freertos_xil_uncached_memory>
)
