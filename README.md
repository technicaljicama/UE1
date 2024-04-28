## What?

Unreal Engine 1 v200 source with modifications to make it run on modern systems.  
Requires assets from the original Unreal v200 retail release. Other versions have not been tested.

## Changes from original source

* Added SDL2 windowing/client driver (NSDLDrv).
* Added GLES2 and fixed pipeline GL graphics drivers (NOpenGLESDrv and NOpenGLDrv).
* Added OpenAL + libxmp audio driver (NOpenALDrv).
* Added GCC support and fixed a bunch of related bugs.
* Still only supports 32-bit Windows builds.
* Editor UI is not supported.

## Building and running

**With GCC:**
```
cmake -Bbuild -G"Unix Makefiles" Source
cmake --build build && cmake --install build --config Debug
```

**With VS2019:**
```
cmake -Bbuild -G"Visual Studio 16 2019" -A Win32 Source
cmake --build build && cmake --install build --config Debug
```

This will copy all resulting DLLs/EXEs to `build/Debug`.

**To run:**
* copy the above DLLs/EXEs to the `System` directory of your Unreal install;  
* copy the configs from `Engine/Config` to the `System` directory;
* run `Unreal.exe`.

## Note

Unreal Engine, Unreal and any related trademarks or copyrights are owned by Epic Games. This repository is not affiliated with or endorsed by Epic Games. 
This is based on the v200 source available elsewhere on the Internet, with assets and third party proprietary libraries removed. 
Do not use for commercial purposes.
