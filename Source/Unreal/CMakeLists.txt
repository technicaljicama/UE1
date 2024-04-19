project(Unreal)

set(SRC_FILES
  "Src/Launch.cpp"
)

if(TARGET_IS_WINDOWS)
  add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})
  if(NOT USE_SDL)
    target_sources(${PROJECT_NAME} PRIVATE "Src/Res/LaunchRes.rc")
  endif()
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
  target_link_libraries(${PROJECT_NAME} NSDLWindow)
elseif(TARGET_IS_WINDOWS)
  target_link_libraries(${PROJECT_NAME} Window)
else()
  message(FATAL_ERROR "Non-Windows platforms can only use NSDLDrv.")
endif()

target_link_libraries(${PROJECT_NAME} Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE UNREAL_EXPORTS)
