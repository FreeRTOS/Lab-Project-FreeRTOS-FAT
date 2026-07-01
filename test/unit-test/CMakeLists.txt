# ------------------------------------------------------------------------------
# FreeRTOS+FAT unit tests (CMock / Unity)
#
# Host-based unit tests for the library's algorithmic code. The module under
# test (ff_ioman.c) is compiled as a "real" library and linked against CMock
# generated mocks of its dependencies (locking, FAT and CRC layers). Lightweight
# kernel type/macro stubs (under include/) stand in for the FreeRTOS kernel.
#
# Usage (matches .github/workflows/release.yml):
#   cmake -S test/unit-test -B test/unit-test/build/
#   make -C test/unit-test/build/ all
#   ctest --test-dir test/unit-test/build/ -E system --output-on-failure
#   make -C test/unit-test/build/ coverage
# ------------------------------------------------------------------------------
cmake_minimum_required( VERSION 3.13 )

project( freertos_plus_fat_utest
         VERSION 1.0.0
         LANGUAGES C )

# Repository root: test/unit-test/ -> test/ -> <root>
get_filename_component( MODULE_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE )
set( UNIT_TEST_DIR "${CMAKE_CURRENT_LIST_DIR}" )
set( CMOCK_DIR "${UNIT_TEST_DIR}/CMock" )

# C90 is the library's baseline; the tests/mocks need a couple of C99 niceties,
# so build everything as C99 without GNU extensions disabled.
set( CMAKE_C_STANDARD 99 )
set( CMAKE_C_STANDARD_REQUIRED ON )

enable_testing()

# ------------------------------------------------------------------------------
# CMock / Unity
# ------------------------------------------------------------------------------
include( ${MODULE_ROOT_DIR}/tools/cmock/create_test.cmake )
include( ${UNIT_TEST_DIR}/cmock_build.cmake )

if( NOT EXISTS ${CMOCK_DIR}/src/cmock.c )
    clone_cmock()
endif()

add_cmock_targets()

# ------------------------------------------------------------------------------
# Shared include directories.
#   - config/  : test FreeRTOSFATConfig.h
#   - include/ : minimal FreeRTOS kernel type/macro stubs (FreeRTOS.h, task.h ...)
#   - <root>/include : the real FreeRTOS+FAT public headers
# The kernel stubs MUST precede the real include dir so they shadow the absent
# kernel headers.
# ------------------------------------------------------------------------------
set( FAT_TEST_INCLUDE_DIRS
     ${UNIT_TEST_DIR}/include
     ${UNIT_TEST_DIR}/config
     ${MODULE_ROOT_DIR}/include )

set( project_name "ff_ioman" )

# =====================  Mocks  ================================================
# Dependencies of ff_ioman.c that are satisfied by mocks at link time.
list( APPEND mock_list
      ${MODULE_ROOT_DIR}/include/ff_locking.h
      ${MODULE_ROOT_DIR}/include/ff_fat.h
      ${MODULE_ROOT_DIR}/include/ff_crc.h )

list( APPEND mock_include_list ${FAT_TEST_INCLUDE_DIRS} )
list( APPEND mock_define_list "" )

# ==================  Module under test  =======================================
# ff_ioman.c is the unit under test; ff_memory.c supplies the (non-inlined) byte
# accessors it calls and is compiled for real rather than mocked.
list( APPEND real_source_files
      ${MODULE_ROOT_DIR}/ff_ioman.c
      ${MODULE_ROOT_DIR}/ff_memory.c )

list( APPEND real_include_directories ${FAT_TEST_INCLUDE_DIRS} )

# =====================  Test code  ============================================
list( APPEND test_include_directories ${FAT_TEST_INCLUDE_DIRS} )

# ==============================================================================
set( mock_name "${project_name}_mock" )
set( real_name "${project_name}_real" )

create_mock_list( ${mock_name}
                  "${mock_list}"
                  "${MODULE_ROOT_DIR}/tools/cmock/project.yml"
                  "${mock_include_list}"
                  "${mock_define_list}" )

create_real_library( ${real_name}
                     "${real_source_files}"
                     "${real_include_directories}"
                     "${mock_name}" )

list( APPEND utest_dep_list ${real_name} )

list( APPEND utest_link_list
      lib${real_name}.a
      -l${mock_name} )

set( utest_name "${project_name}_utest" )
set( utest_source "${UNIT_TEST_DIR}/ff_ioman_utest.c" )

create_test( ${utest_name}
             ${utest_source}
             "${utest_link_list}"
             "${utest_dep_list}"
             "${test_include_directories}" )
set( coverage_dep_list ${utest_name} )

set( project_name "ff_file" )

# =====================  Mocks  ================================================
# Keep the ff_file unit focused on FF_Close().  The module is built with
# function sections so the linker can discard unrelated file APIs and their
# dependencies.
set( mock_list "" )
list( APPEND mock_list
      ${MODULE_ROOT_DIR}/include/ff_locking.h
      ${MODULE_ROOT_DIR}/include/ff_dir.h
      ${MODULE_ROOT_DIR}/include/ff_fat.h
      ${MODULE_ROOT_DIR}/include/ff_ioman.h
      ${MODULE_ROOT_DIR}/include/ff_time.h )

set( mock_include_list ${FAT_TEST_INCLUDE_DIRS} )
set( mock_define_list "" )

# ==================  Module under test  =======================================
set( real_source_files "" )
list( APPEND real_source_files
      ${MODULE_ROOT_DIR}/ff_file.c )

set( real_include_directories ${FAT_TEST_INCLUDE_DIRS} )

# =====================  Test code  ============================================
set( test_include_directories ${FAT_TEST_INCLUDE_DIRS} )

# ==============================================================================
set( mock_name "${project_name}_mock" )
set( real_name "${project_name}_real" )

create_mock_list( ${mock_name}
                  "${mock_list}"
                  "${MODULE_ROOT_DIR}/tools/cmock/project.yml"
                  "${mock_include_list}"
                  "${mock_define_list}" )

create_real_library( ${real_name}
                     "${real_source_files}"
                     "${real_include_directories}"
                     "${mock_name}" )
target_compile_options( ${real_name} PRIVATE -ffunction-sections -fdata-sections )

set( utest_dep_list "" )
list( APPEND utest_dep_list ${real_name} )

set( utest_link_list "" )
list( APPEND utest_link_list
      lib${real_name}.a
      -l${mock_name}
      -Wl,--gc-sections )

set( utest_name "${project_name}_utest" )
set( utest_source "${UNIT_TEST_DIR}/ff_file_utest.c" )

create_test( ${utest_name}
             ${utest_source}
             "${utest_link_list}"
             "${utest_dep_list}"
             "${test_include_directories}" )
list( APPEND coverage_dep_list ${utest_name} )

# ------------------------------------------------------------------------------
# `coverage` target: run the tests and collect lcov data into coverage.info.
# ------------------------------------------------------------------------------
add_custom_target( coverage
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}
            -P ${MODULE_ROOT_DIR}/tools/cmock/coverage.cmake
    DEPENDS ${coverage_dep_list}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running unit tests and collecting coverage" )
