add_library(freertos_plus_fat_port STATIC)

target_sources(freertos_plus_fat_port
    PRIVATE
    ff_sddisk.c
    ff_socfpga.c
    ${FREERTOS_TOP_DIR}/tinyusb/apps/msc_app.c
    )

target_include_directories(freertos_plus_fat_port
    PUBLIC
    .
    ${FREERTOS_TOP_DIR}/drivers/sdmmc/
    )

target_link_libraries(freertos_plus_fat_port
  PUBLIC
    freertos_plus_fat_port_common
  PRIVATE
    agilex5-tinyusb
    freertos_kernel
    freertos_plus_fat
    socfpga_drivers
    )
