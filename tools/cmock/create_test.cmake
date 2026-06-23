# CMake helpers for building CMock/Unity based unit tests.
#
# Adapted from the FreeRTOS core library repositories (originally from the
# amazon-freertos repository). The "real library" warning flags have been
# relaxed relative to the upstream copy: the FreeRTOS+FAT sources predate the
# strict -Werror/-Wconversion regime used by the newer core libraries, so the
# aggressive set is not applied here. Coverage instrumentation is retained.

# Function to create the test executable.
function(create_test test_name
                     test_src
                     link_list
                     dep_list
                     include_list)
    set(mocks_dir "${CMAKE_CURRENT_BINARY_DIR}/mocks")
    include(CTest)
    get_filename_component(test_src_absolute ${test_src} ABSOLUTE)
    add_custom_command(OUTPUT ${test_name}_runner.c
                       COMMAND ruby
                       ${CMOCK_DIR}/vendor/unity/auto/generate_test_runner.rb
                       ${MODULE_ROOT_DIR}/tools/cmock/project.yml
                       ${test_src_absolute}
                       ${test_name}_runner.c
                       DEPENDS ${test_src}
        )
    add_executable(${test_name} ${test_src} ${test_name}_runner.c)
    set_target_properties(${test_name} PROPERTIES
                          COMPILE_FLAG "-O0 -ggdb"
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
                          INSTALL_RPATH_USE_LINK_PATH TRUE
        )
    # The module under test is built with coverage instrumentation, so the test
    # executable must link the coverage runtime.
    target_link_options(${test_name} PRIVATE --coverage)
    target_include_directories(${test_name} PUBLIC
                               ${mocks_dir}
                               ${include_list}
        )

    target_link_directories(${test_name} PUBLIC
                            ${CMAKE_CURRENT_BINARY_DIR}
        )

    # Link all libraries sent through parameters.
    foreach(link IN LISTS link_list)
        target_link_libraries(${test_name} ${link})
    endforeach()

    # Add a dependency to each entry of the dep_list parameter.
    foreach(dependency IN LISTS dep_list)
        add_dependencies(${test_name} ${dependency})
        target_link_libraries(${test_name} ${dependency})
    endforeach()
    target_link_libraries(${test_name} unity)
    target_link_directories(${test_name} PUBLIC
                            ${CMAKE_CURRENT_BINARY_DIR}/lib
        )
    add_test(NAME ${test_name}
             COMMAND ${CMAKE_BINARY_DIR}/bin/tests/${test_name}
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
endfunction()

# Generates a mock library based on a module's header file, placing the
# generated source in the build directory.
#   mock_name         : target name for the mock library
#   mock_list         : list of header files to mock
#   cmock_config      : CMock configuration (project.yml)
#   mock_include_list : include directories required to compile the mocks
#   mock_define_list  : extra compile definitions for the mocks
function(create_mock_list mock_name
                          mock_list
                          cmock_config
                          mock_include_list
                          mock_define_list)
    set(mocks_dir "${CMAKE_CURRENT_BINARY_DIR}/mocks")
    add_library(${mock_name} SHARED)
    foreach(mock_file IN LISTS mock_list)
        get_filename_component(mock_file_abs ${mock_file} ABSOLUTE)
        get_filename_component(mock_file_name ${mock_file} NAME_WLE)
        get_filename_component(mock_file_dir ${mock_file} DIRECTORY)
        add_custom_command(
            OUTPUT ${mocks_dir}/mock_${mock_file_name}.c
            COMMAND ruby
            ${CMOCK_DIR}/lib/cmock.rb
            -o${cmock_config} ${mock_file_abs}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            )
        target_sources(${mock_name} PUBLIC
                       ${mocks_dir}/mock_${mock_file_name}.c
            )
        target_include_directories(${mock_name} PUBLIC ${mock_file_dir})
    endforeach()
    target_include_directories(${mock_name} PUBLIC
                               ${mocks_dir}
                               ${mock_include_list}
        )
    if(APPLE)
        set_target_properties(${mock_name} PROPERTIES
                              LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib
                              POSITION_INDEPENDENT_CODE ON
                              LINK_FLAGS "-Wl,-undefined,dynamic_lookup"
            )
    else()
        set_target_properties(${mock_name} PROPERTIES
                              LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib
                              POSITION_INDEPENDENT_CODE ON
            )
    endif()
    target_compile_definitions(${mock_name} PUBLIC ${mock_define_list})
    target_link_libraries(${mock_name} cmock unity)
endfunction()

# Builds the module under test as a static library, instrumented for coverage.
function(create_real_library target
                             src_file
                             real_include_list
                             mock_name)
    add_library(${target} STATIC ${src_file})
    target_include_directories(${target} PUBLIC ${real_include_list})
    set_target_properties(${target} PROPERTIES
        COMPILE_FLAGS "-Wall -Wextra \
                -Wno-unused-but-set-variable \
                -Wno-unused-parameter \
                -fprofile-arcs -ftest-coverage \
                -fno-inline \
                -fno-optimize-sibling-calls \
                -O0"
        LINK_FLAGS "-fprofile-arcs -ftest-coverage"
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib
        )
    if(NOT(mock_name STREQUAL ""))
        add_dependencies(${target} ${mock_name})
    endif()
endfunction()
