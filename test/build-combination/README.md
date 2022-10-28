# Build Instructions

This test aims at finding only compilation issues and as a result, the
generated binary is not runnable.

## UNIX (Linux and Mac)

All the CMake commands are to be run from the root of the repository.

* Build checks (Default configuration)
```
cmake -S . -B build -DFREERTOS_PLUS_FAT_TEST_CONFIGURATION=DEFAULT_CONF
make -C .
```

## Windows

All the CMake commands are to be run from the root of the repository.

* Build checks (Default configuration)
```
cmake -S . -B build -DFREERTOS_PLUS_FAT_TEST_CONFIGURATION=DEFAULT_CONF -DCMAKE_GENERATOR_PLATFORM=Win32
```
Open the generated Visual Studio Solution file `test\build-combination\build\FreeRTOS-Plus-FAT Build Combination.sln`
in Visual Studio and click `Build --> Build Solution`.
