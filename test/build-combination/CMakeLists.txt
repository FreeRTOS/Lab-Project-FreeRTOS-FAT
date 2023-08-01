add_library( freertos_plus_fat_config_common INTERFACE )
target_include_directories(freertos_plus_fat_config_common INTERFACE Common/include )

# -------------------------------------------------------------------
add_library( freertos_plus_fat_config_default INTERFACE)
target_include_directories(freertos_plus_fat_config_default INTERFACE DefaultConf)
target_link_libraries(freertos_plus_fat_config_default INTERFACE freertos_plus_fat_config_common)

# -------------------------------------------------------------------
# Configuration for FreeRTOS-Plus-FAT tests
if(FREERTOS_PLUS_FAT_TEST_CONFIGURATION STREQUAL "CUSTOM" )
    # Check Config target is available. And then do nothing.
    if(NOT TARGET freertos_config )
        message(FATAL_ERROR "FREERTOS_PLUS_FAT_TEST_CONFIGURATION = CUSTOM, but no freertos_config target defined.")
    endif()
else()
    add_library( freertos_config ALIAS freertos_plus_fat_config_default)
endif()

add_executable(freertos_plus_fat_build_test EXCLUDE_FROM_ALL)

target_sources(freertos_plus_fat_build_test
  PRIVATE
    Common/main.c
)

target_compile_options(freertos_plus_fat_build_test
  PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-missing-noreturn>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-missing-prototypes>
)

target_link_libraries(freertos_plus_fat_build_test
  PRIVATE
    freertos_plus_fat
    freertos_plus_fat_port
    freertos_kernel
)
