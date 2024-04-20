project(Unreal)

if(USE_SDL)
  set(SRC_FILES "Src/SDLLaunch.cpp")
else()
  set(SRC_FILES "Src/WinLaunch.cpp")
endif()

if(TARGET_IS_WINDOWS)
  list(APPEND SRC_FILES "Src/Res/LaunchRes.rc")
  add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})
else()
  add_executable(${PROJECT_NAME} ${SRC_FILES})
endif()

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

if(USE_SDL)
  target_link_libraries(${PROJECT_NAME} NSDLDrv)
elseif(TARGET_IS_WINDOWS)
  target_link_libraries(${PROJECT_NAME} Window)
else()
  message(FATAL_ERROR "Non-Windows platforms can only use NSDLDrv.")
endif()

target_link_libraries(${PROJECT_NAME} Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE UNREAL_EXPORTS)
