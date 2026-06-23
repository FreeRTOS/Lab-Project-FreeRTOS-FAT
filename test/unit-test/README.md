# FreeRTOS+FAT unit tests

Host-based unit tests built with [Unity] and [CMock], following the same
pattern as the FreeRTOS core libraries (for example [coreMQTT]).

The module under test (`ff_ioman.c`) is compiled as a "real" library and linked
against CMock-generated mocks of its dependencies (the locking, FAT and CRC
layers). Lightweight kernel type/macro stubs (under `include/`) stand in for the
FreeRTOS kernel, and the FreeRTOS+FAT byte accessors and buffer cache run for
real, fed by an in-memory block device.

[Unity]: https://github.com/ThrowTheSwitch/Unity
[CMock]: https://github.com/ThrowTheSwitch/CMock
[coreMQTT]: https://github.com/FreeRTOS/coreMQTT/tree/main/test/unit-test

## Layout

| Path | Purpose |
| --- | --- |
| `CMock/` | CMock submodule (vendors Unity + CException). |
| `CMakeLists.txt` | Top-level test build: sets up CMock/Unity, declares the mocks, the module under test, and the `coverage` target. |
| `cmock_build.cmake` | Clones CMock and builds the `unity` / `cmock` libraries. |
| `config/FreeRTOSFATConfig.h` | Test configuration. `ffconfigMAX_PARTITIONS` is 4 so the partition-enumeration bounds checks are reachable with a compact disk image. |
| `include/` | Minimal `FreeRTOS.h`, `task.h`, `semphr.h`, `event_groups.h` stubs (types/macros only), shadowing the absent kernel headers. |
| `ff_ioman_utest.c` | Unity tests for partition-table parsing in `ff_ioman.c`. |

Shared CMake helpers live at the repository root under
[`tools/cmock/`](../../tools/cmock): `create_test.cmake` (the
`create_mock_list` / `create_real_library` / `create_test` functions),
`project.yml` (CMock configuration) and `coverage.cmake`.

## Prerequisites

- CMake, a C compiler, and `make`
- Ruby (CMock and the Unity test-runner generator are Ruby scripts)
- `lcov` for the coverage target
- The `CMock` submodule (`git submodule update --init --recursive`). The build
  will attempt to clone it automatically if missing.

## Running

```sh
cmake -S test/unit-test -B test/unit-test/build/
make -C test/unit-test/build/ all
ctest --test-dir test/unit-test/build/ -E system --output-on-failure
make -C test/unit-test/build/ coverage   # writes build/coverage.info
```

These are the same steps the release workflow (`.github/workflows/release.yml`)
runs.

## What `ff_ioman_utest` covers

The suite drives the public `FF_PartitionSearch()`, which walks the extended /
logical partition (EBR) chain via the internal `FF_ParseExtended()`. It focuses
on the bounds checks that keep `FF_SPartFound_t::pxPartitions[]` from being
written past `ffconfigMAX_PARTITIONS`:

- **Overflowing chain is clamped** — a disk advertising more logical partitions
  than configured stops at the limit (and does not overrun the array).
- **Chain at the limit is recorded** — exactly `ffconfigMAX_PARTITIONS` logical
  partitions are all enumerated.
- **Self-referential chain terminates** — an EBR that links to itself is capped
  rather than looping forever and overrunning the array.

Each test wraps `FF_SPartFound_t` in a guard structure and asserts the guard
bytes are untouched, so an out-of-bounds write is detected as a test failure.

The locking layer is mocked and ignored (`FF_PendSemaphore_Ignore()` etc.);
`FF_CreateEvents_IgnoreAndReturn( pdTRUE )` lets the I/O manager be created.

## Adding more tests

1. Add the test source and declare it in `CMakeLists.txt` via `create_test`.
2. If the unit under test pulls in new dependencies, add their headers to
   `mock_list` so CMock generates mocks for them, and add any extra real
   sources to `real_source_files`.
3. Write `test_*` functions using Unity assertions plus `setUp`/`tearDown`; the
   test runner is generated automatically.
