cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0048 NEW) # project version
cmake_policy(SET CMP0076 NEW) # full paths

########################################################################
# Project Details
project(FreeRTOS-Plus-TCP
        VERSION 0.0.1
        DESCRIPTION "FreeRTOS DOS Compatible Embedded FAT File System"
        HOMEPAGE_URL https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_FAT/index.html
        LANGUAGES C)

# Do not allow in-source build.
if( ${PROJECT_SOURCE_DIR} STREQUAL ${PROJECT_BINARY_DIR} )
message( FATAL_ERROR "In-source build is not allowed. Please build in a separate directory, such as ${PROJECT_SOURCE_DIR}/build." )
endif()

# Options

# Optional:  FREERTOS_PLUS_FAT_DEV_SUPPORT 
#   - when OFF - device support is disabled and not used.
#   - When ON  - device support is enabled and the ff_devices.h API is used.
# Optional:  FREERTOS_PLUS_FAT_PORT
#   - When not defined - identifies native platform Linxu or MinGW and uses that port.
#   - When defined as A_CUSTOM_PORT - the port library must be defined in advance.
#   - When any of the other supported ports - the port library is defined by portable source files.
option(FREERTOS_PLUS_FAT_DEV_SUPPORT "FreeRTOS Plus FAT Device selection support" OFF)

# Select the appropriate FAT Port
# This will fail the CMake preparation step if not set to one of those values.
set(FREERTOS_PLUS_FAT_PORT "" CACHE STRING "FreeRTOS Plus FAT Port selection")
set(FREERTOS_PLUS_FAT_PORT_LIST
    A_CUSTOM_PORT
    ATSAM4E    
    AVR32_UC3  
    LPC18XX
    POSIX    
    STM32FXX   
    STM32HXX   
    ZYNQ       
    ZYNQ_2019_3
)
if(NOT FREERTOS_PLUS_FAT_PORT)
    # Attempt to detect the system.
    if(UNIX)
        message(STATUS "Detected UNIX/Posix system setting FREERTOS_PLUS_TCP_NETWORK_IF = POSIX")
        set(FREERTOS_PLUS_FAT_PORT POSIX)
    elseif(MINGW)
        message(STATUS "Detected Windows MinGW system setting FREERTOS_PLUS_TCP_NETWORK_IF = WIN_MGW")
        set(FREERTOS_PLUS_FAT_PORT WIN_PCAP)
    endif()
endif()

if(NOT FREERTOS_PLUS_FAT_PORT IN_LIST FREERTOS_PLUS_FAT_PORT_LIST )
    message(FATAL_ERROR " FREERTOS_PLUS_FAT_PORT is '${FREERTOS_PLUS_FAT_PORT}'.\n"
        " Please specify it from top-level CMake file (example):\n"
        "   set(FREERTOS_PLUS_FAT_PORT POSIX CACHE STRING \"\")\n"
        " or from CMake command line option:\n"
        "   -DFREERTOS_PLUS_FAT_PORT=POSIX\n"
        " \n"
        " Available port options: (Tested means compiled with that variant)\n"
        " A_CUSTOM_PORT          Target: User Defined\n"
        " ATSAM4E                Target: ATSAM4E        Tested: TODO\n"
        " AVR32_UC3              Target: avr32_uc3      Tested: TODO\n"
        " LPC18XX                Target: lpc18xx        Tested: TODO\n"
        " POSIX                  Target: linux/Posix\n"
        " STM32F4XX              Target: STM32F4xx      Tested: TODO\n"
        " STM32F7XX              Target: STM32F7xx      Tested: TODO\n"
        " ZYNQ                   Target: Xilinx Zynq    Tested: TODO\n"
        " ZYNQ_2019_3            Target: Xilinx Zynq 2019.3")
elseif((FREERTOS_PLUS_FAT_PORT STREQUAL "A_CUSTOM_PORT") AND (NOT TARGET freertos_plus_fat_port) )
    message(FATAL_ERROR " FREERTOS_PLUS_FAT_PORT is set to A_CUSTOM_PORT.\n"
        " Please specify the custom port target with all necessary files.\n"
        " For example, assuming a directory of:\n"
        "  FreeRTOSPlusFatPort/\n"
        "    CMakeLists.txt\n"
        "    ff_sddisk.c\n"
        " Where FreeRTOSPlusFatPort/CMakeLists.txt is a modified version of:\n"
        "   add_library(freertos_plus_fat_port STATIC)\n"
        "   target_sources(freertos_plus_fat_port\n"
        "     PRIVATE\n"
        "       ff_sddisk.c)\n"
        "   target_link_libraries(freertos_plus_fat_port\n"
        "     PUBLIC\n"
        "       freertos_plus_fat_port_common\n"
        "     PRIVATE\n"
        "       freertos_kernel\n"
        "       freertos_plus_fat)")
endif()

add_library( freertos_plus_fat STATIC )

target_sources( freertos_plus_fat
  PRIVATE
    include/ff_crc.h
    include/ff_devices.h
    include/ff_dir.h
    include/ff_error.h
    include/ff_fat.h
    include/ff_fatdef.h
    include/ff_file.h
    include/ff_format.h
    include/ff_headers.h
    include/ff_ioman.h
    include/ff_locking.h
    include/ff_memory.h
    include/ff_old_config_defines.h
    include/ff_stdio.h
    include/ff_string.h
    include/ff_sys.h
    include/ff_time.h
    include/FreeRTOS_errno_FAT.h
    include/FreeRTOSFATConfigDefaults.h

    ff_crc.c
    $<$<BOOL:${FREERTOS_PLUS_FAT_DEV_SUPPORT}>:ff_dev_support.c>
    ff_dir.c
    ff_error.c
    ff_fat.c
    ff_file.c
    ff_format.c
    ff_ioman.c
    ff_locking.c
    ff_memory.c
    ff_stdio.c
    ff_string.c
    ff_sys.c
    ff_time.c
)

target_include_directories( freertos_plus_fat SYSTEM
  PUBLIC
    include
)

target_compile_definitions( freertos_plus_fat
  PUBLIC
    ffconfigDEV_SUPPORT=$<BOOL:${FREERTOS_PLUS_FAT_DEV_SUPPORT}>
)

target_compile_options( freertos_plus_fat
  PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-cast-qual>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-constant-conversion>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-covered-switch-default>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-documentation-unknown-command>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-documentation>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-extra-semi-stmt>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wno-format>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-implicit-int-conversion>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-missing-variable-declarations>
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-Wno-overflow>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-padded>
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-Wno-pedantic>

    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-reserved-macro-identifier>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-shorten-64-to-32>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-sign-conversion>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-tautological-constant-out-of-range-compare>
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-Wno-type-limits>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-undef>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-unused-macros>
)

target_link_libraries( freertos_plus_fat
  PUBLIC
    freertos_config
  PRIVATE
    freertos_plus_fat_port
    freertos_kernel
)

add_subdirectory(portable)