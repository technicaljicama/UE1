cmake_minimum_required(VERSION 3.15)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project(UE1 CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS ON)

if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
endif()

# Options

option(USE_SDL "Use NSDLDrv instead of WinDrv" OFF)
option(BUILD_EDITOR "Build Editor (Windows only)" OFF)
option(BUILD_SOFTDRV "Build SoftDrv (x86 only)" OFF)
option(BUILD_NULLSOUNDDRV "Build SoundDrv (Null driver)" ON)

# Determine target arch and get normalized arch string

include(TargetArch)
target_architecture(TARGET_ARCH)

set(TARGET_IS_X86 FALSE)
set(TARGET_IS_ARM FALSE)
set(TARGET_IS_64BIT FALSE)
set(TARGET_IS_BIG_ENDIAN FALSE)

if(TARGET_ARCH MATCHES "arm")
  set(TARGET_IS_ARM TRUE)
  if(TARGET_ARCH MATCHES "64")
    # arm64/aarch64
    set(TARGET_IS_64BIT TRUE)
    set(TARGET_ARCH "arm64")
  else()
    # arm
    set(TARGET_ARCH "arm")
  endif()
elseif((TARGET_ARCH MATCHES "86") OR (TARGET_ARCH STREQUAL "ia64"))
  set(TARGET_IS_X86 TRUE)
  if(TARGET_ARCH MATCHES "64")
    # x86_64/amd64
    set(TARGET_IS_64BIT TRUE)
    set(TARGET_ARCH "x86_64")
  else()
    # x86
    set(TARGET_ARCH "x86")
  endif()
endif()

message(STATUS "Target arch: ${TARGET_ARCH}")
message(STATUS "64-bit: ${TARGET_IS_64BIT}")

if(TARGET_IS_64BIT)
  # TODO
  message(FATAL_ERROR "64-bit platforms are currently not supported. If you're build on Windows with MSVC, try -A Win32.")
endif()

# Determine target OS/platform

set(TARGET_IS_WINDOWS FALSE)
set(TARGET_IS_UNIX FALSE)
if(WIN32)
  set(TARGET_IS_WINDOWS TRUE)
else()
  # TODO
  set(TARGET_IS_UNIX TRUE)
endif()

message(STATUS "Windows target: ${TARGET_IS_WINDOWS}")

# Common compiler options

if(TARGET_IS_64BIT)
  add_definitions(-DPLATFORM_64BIT)
else()
  add_definitions(-DPLATFORM_32BIT)
endif()

if(TARGET_IS_X86)
  add_definitions(-DPLATFORM_X86)
elseif(TARGET_IS_ARM)
  add_definitions(-DPLATFORM_ARM)
endif()

if(TARGET_IS_BIG_ENDIAN)
  add_definitions(-DPLATFORM_BIG_ENDIAN)
else()
  add_definitions(-DPLATFORM_LITTLE_ENDIAN)
endif()

if(TARGET_IS_WINDOWS)
  add_definitions(-DPLATFORM_WIN32)
  add_definitions(-DWIN32)
  add_definitions(-DWINDOWS_IGNORE_PACKING_MISMATCH)
else()
  # TODO
  add_definitions(-DPLATFORM_POSIX)
endif()

if(MSVC)
  message(STATUS "Building ${CMAKE_BUILD_TYPE} with MSVC")
  add_definitions(-DPLATFORM_MSVC)
  add_compile_options(/nologo /fp:precise /analyze- /MP /MD /Gd /Gy /GS /Gm- /EHsc /Zi /Zc:forScope- /Zc:wchar_t /Zc:inline /Zp4)
  add_compile_options(/wd4291 /wd4297)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(/Od)
  else()
    add_compile_options(/GL /O2)
  endif()
else()
  message(STATUS "Building ${CMAKE_BUILD_TYPE} with GCC or Clang")
  add_compile_options(-ffunction-sections -fpack-struct=4 -fpermissive)
  add_compile_options(-Wno-write-strings)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Og)
  else()
    add_compile_options(-O2)
  endif()
endif()

# Subprojects

set(INSTALL_TARGETS Core Engine Render IpDrv Fire Unreal)

set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_subdirectory(Core)
add_subdirectory(Engine)
add_subdirectory(Render)
add_subdirectory(IpDrv)
add_subdirectory(Fire)

if(USE_SDL)
  # SDL2
  # add_subdirectory(NSDLWindow)
  # add_subdirectory(NSDLDrv)
  # list(APPEND INSTALL_TARGETS NSDLWindow NSDLDrv)
else()
  # archaic WinAPI shit
  add_subdirectory(Window)
  add_subdirectory(WinDrv)
  list(APPEND INSTALL_TARGETS Window WinDrv)
endif()

if(BUILD_SOFTDRV)
  add_subdirectory(SoftDrv)
  list(APPEND INSTALL_TARGETS SoftDrv)
endif()

if(BUILD_NULLSOUNDDRV)
  add_subdirectory(SoundDrv)
  list(APPEND INSTALL_TARGETS SoundDrv)
endif()

if(BUILD_EDITOR)
  # this requires WinDrv
  add_subdirectory(Editor)
  list(APPEND INSTALL_TARGETS Editor)
endif()

add_subdirectory(Unreal)

# assemble all modules and pdbs in one dir

set(INSTALL_FILES )

foreach(TGT ${INSTALL_TARGETS})
  list(APPEND INSTALL_FILES "$<TARGET_FILE:${TGT}>")
  if(MSVC)
    list(APPEND INSTALL_FILES "$<TARGET_PDB_FILE:${TGT}>")
  endif()
endforeach()

install(FILES ${INSTALL_FILES} DESTINATION "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
