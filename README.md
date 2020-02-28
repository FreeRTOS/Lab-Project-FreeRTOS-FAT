## FreeRTOS+FAT: DOS Compatible Embedded FAT File System

FreeRTOS+FAT is an open source, thread aware and scalable FAT12/FAT16/FAT32 DOS/Windows compatible embedded FAT file system which was recently acquired by [Real Time Engineers ltd](). for use with and without the RTOS.

FreeRTOS+FAT is already used in commercial products, and is the file system used in the [FTP](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/FTP_Server.html) and [HTTP](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/HTTP_web_Server.html) server examples that are documented on the [FreeRTOS+TCP](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/index.html) pages.

The [standard C library style API](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_FAT/Standard_File_System_API.html) includes a thread local errno value, and the lower level native API provides a rich set of detailed error codes.

For more details, please visit [FreeRTOS+FAT](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_FAT/index.html) page. 

## To consume FreeRTOS+FAT
It is recommended to use this repository as a submodule. Please refer to [Git Tools — Submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules). 

## Notes
This project is undergoing optimizations or refactorization to improve memory usage, modularity, documentation, demo usability, or test coverage. 