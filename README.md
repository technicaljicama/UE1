## What?

Unreal Engine 1 v200 source with modifications to make it run on modern systems.  
Requires assets from the original Unreal v200 retail release. Other versions have not been tested.

## Building and running

* Currently only Visual Studio 2019 Win32 builds work correctly.
* GCC builds are a work in progress.
* Editor UI is not supported.
* There is currently no  working audio driver.

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
