cmake_minimum_required(VERSION 3.15)

project(UnrealLoader CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS ON)

if(NOT VITA)
  message(FATAL_ERROR "This loader is for the PSVita only.")
endif()

if(BUILD_FOR_PSVITA)
  message(FATAL_ERROR "Use BUILD_FOR_PSVITA to build an armhf-linux version of the main game, then build this loader for the PSVita.")
endif()

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Unreal")
set(VITA_TITLEID  "UNRL00001")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")

set(SRC_FILES "Src/Loader.cpp" "Src/Exports.cpp")

add_definitions(-DPLATFORM_ARM -DPLATFORM_POSIX -DPLATFORM_PSVITA -DPLATFORM_LITTLE_ENDIAN -DPLATFORM_SDL)
add_compile_options(-O2 -fsigned-char -fno-short-enums -ffunction-sections -fno-optimize-sibling-calls -Wno-write-strings -Wl,-q)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
  ${CMAKE_CURRENT_SOURCE_DIR}/../Core/Inc
)

target_link_libraries(${PROJECT_NAME}
  -Wl,--whole-archive pthread vrtld -Wl,--no-whole-archive
  openal
  xmp
  SDL2
  vitaGL
  vitashark
  mathneon
  SceShaccCgExt
  -Wl,--whole-archive stdc++ -Wl,--no-whole-archive
  taihen_stub
  kubridge_stub
  SceNet_stub
  SceNetCtl_stub
  SceAppMgr_stub
  SceAppUtil_stub
  SceAudio_stub
  SceAudioIn_stub
  SceCtrl_stub
  SceCommonDialog_stub
  SceDisplay_stub
  SceFios2_stub
  SceGxm_stub
  SceIme_stub
  SceShaccCg_stub
  SceSysmodule_stub
  ScePower_stub
  SceTouch_stub
  SceMotion_stub
  SceHid_stub
  SceVshBridge_stub
  SceSblSsMgr_stub
  SceKernelDmacMgr_stub
)

vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE GEN_EXPORTS ${PROJECT_NAME}.yml)
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/Res/sce_sys sce_sys
)
