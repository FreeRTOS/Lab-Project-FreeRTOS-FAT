# Runs the unit tests and collects lcov coverage for the FreeRTOS+FAT sources.
# Adapted from the FreeRTOS core library repositories. Coverage is scoped to the
# library sources under test (ff_*.c) rather than an upstream "source" tree.
cmake_minimum_required(VERSION 3.13)

# Reset coverage counters and prepare the output directory.
execute_process(
    COMMAND lcov --directory ${CMAKE_BINARY_DIR}
                 --base-directory ${CMAKE_BINARY_DIR}
                 --zerocounters
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
    )

# Baseline (zeroed) capture so unexecuted lines still appear in the report.
execute_process(
    COMMAND lcov --directory ${CMAKE_BINARY_DIR}
                 --base-directory ${CMAKE_BINARY_DIR}
                 --initial
                 --capture
                 --rc branch_coverage=1
                 --output-file ${CMAKE_BINARY_DIR}/base_coverage.info
                 --include "*ff_*.c"
    )

file(GLOB files "${CMAKE_BINARY_DIR}/bin/tests/*")

set(REPORT_FILE ${CMAKE_BINARY_DIR}/utest_report.txt)
file(WRITE ${REPORT_FILE} "")

# Run every test executable, collecting their output for CI logs.
foreach(testname ${files})
    get_filename_component(test ${testname} NAME_WLE)
    message("Running ${testname}")
    execute_process(COMMAND ${testname}
                    OUTPUT_FILE ${CMAKE_BINARY_DIR}/${test}_out.txt)
    file(READ ${CMAKE_BINARY_DIR}/${test}_out.txt CONTENTS)
    file(APPEND ${REPORT_FILE} "${CONTENTS}")
endforeach()

# Capture coverage after running the tests.
execute_process(
    COMMAND lcov --capture
                 --rc branch_coverage=1
                 --base-directory ${CMAKE_BINARY_DIR}
                 --directory ${CMAKE_BINARY_DIR}
                 --output-file ${CMAKE_BINARY_DIR}/second_coverage.info
                 --include "*ff_*.c"
    )

# Combine the baseline with the post-run data.
execute_process(
    COMMAND lcov --base-directory ${CMAKE_BINARY_DIR}
                 --directory ${CMAKE_BINARY_DIR}
                 --add-tracefile ${CMAKE_BINARY_DIR}/base_coverage.info
                 --add-tracefile ${CMAKE_BINARY_DIR}/second_coverage.info
                 --output-file ${CMAKE_BINARY_DIR}/coverage.info
                 --rc branch_coverage=1
                 --include "*ff_*.c"
    )
