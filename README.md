# FreeRTOS+FAT: DOS Compatible Embedded FAT File System

FreeRTOS+FAT is an open source, thread aware and scalable FAT12/FAT16/FAT32 DOS
/Windows compatible embedded FAT file system which was recently acquired by
[Real Time Engineers ltd](). for use with and without FreeRTOS.

FreeRTOS+FAT is already used in commercial products, and is the file system
used in the
[FTP](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/FTP_Server.html)
and
[HTTP](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/HTTP_web_Server.html)
server examples that are documented on the
[FreeRTOS+TCP](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/index.html)
pages.

The
[standard C library style API](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_FAT/Standard_File_System_API.html)
includes a thread local errno value, and the lower level native API provides a
rich set of detailed error codes.

For more details, please visit
[FreeRTOS+FAT](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_FAT/index.html) page.

## To consume FreeRTOS+FAT

### Consume with CMake

If using CMake, it is recommended to use this repository using FetchContent.
Add the following into your project's main or a subdirectory's `CMakeLists.txt`:

```cmake
include(FetchContent)

FetchContent_Declare( freertos_plus_fat
  GIT_REPOSITORY https://github.com/FreeRTOS/Lab-Project-FreeRTOS-FAT.git
  GIT_TAG        master #Note: Best practice to use specific git-hash or tagged version
  GIT_SUBMODULES "" # Don't grab any submodules since not latest
)

# ...

set( FREERTOS_PLUS_FAT_DEV_SUPPORT OFF CACHE BOOL "" FORCE)
# Select the native compile PORT
set( FREERTOS_PLUS_FAT_PORT "POSIX" CACHE STRING "" FORCE)
# Select the cross-compile PORT
if (CMAKE_CROSSCOMPILING)
  # Eg. Zynq 2019_3 version of port
  set(FREERTOS_PLUS_FAT_PORT "ZYNQ_2019_3" CACHE STRING "" FORCE)
endif()

FetchContent_MakeAvailable(freertos_plus_fat)
```

If you already have FreeRTOS in your project, you may skip the fetch content by setting
`FREERTOS_PLUS_FAT_FETCH_FREERTOS` to `OFF`.

### Consuming stand-alone

It is recommended to use this repository as a submodule. Please refer to
[Git Tools — Submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules).

## Notes

This project is undergoing optimizations or refactoring to improve memory usage,
modularity, documentation, demo usability, or test coverage.
