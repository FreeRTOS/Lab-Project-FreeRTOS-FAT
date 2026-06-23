# Macro utilities to build the Unity and CMock libraries used by the unit tests.
# Adapted from the FreeRTOS core library repositories.

# Clone the CMock submodule (including its Unity / CException sub-submodules) if
# it has not been checked out yet.
macro(clone_cmock)
    find_package(Git REQUIRED)
    message("Cloning submodule CMock.")
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --checkout --init --recursive ${CMOCK_DIR}
                    WORKING_DIRECTORY ${MODULE_ROOT_DIR}
                    RESULT_VARIABLE CMOCK_CLONE_RESULT)

    if(NOT ${CMOCK_CLONE_RESULT} STREQUAL "0")
        message(FATAL_ERROR "Failed to clone CMock submodule.")
    endif()
endmacro()

# Add the Unity and CMock library targets to the build.
macro(add_cmock_targets)
    list(APPEND CMOCK_INCLUDE_DIRS
         "${CMOCK_DIR}/vendor/unity/src/"
         "${CMOCK_DIR}/vendor/unity/extras/fixture/src"
         "${CMOCK_DIR}/vendor/unity/extras/memory/src"
         "${CMOCK_DIR}/src"
        )

    add_library(cmock STATIC
                "${CMOCK_DIR}/src/cmock.c"
        )

    set_target_properties(cmock PROPERTIES
                          ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
                          POSITION_INDEPENDENT_CODE ON
                          COMPILE_FLAGS "-Og"
        )

    target_include_directories(cmock PUBLIC
                               ${CMOCK_DIR}/src
                               ${CMOCK_DIR}/vendor/unity/src/
                               ${CMOCK_DIR}/examples
                               ${CMOCK_INCLUDE_DIRS}
        )

    add_library(unity STATIC
                "${CMOCK_DIR}/vendor/unity/src/unity.c"
                "${CMOCK_DIR}/vendor/unity/extras/fixture/src/unity_fixture.c"
                "${CMOCK_DIR}/vendor/unity/extras/memory/src/unity_memory.c"
        )

    set_target_properties(unity PROPERTIES
                          ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
                          POSITION_INDEPENDENT_CODE ON
        )

    target_include_directories(unity PUBLIC
                               ${CMOCK_INCLUDE_DIRS}
        )

    target_link_libraries(cmock unity)
endmacro()
