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
  target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY} NSDLDrv)
  target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIR})
elseif(TARGET_IS_WINDOWS)
  target_link_libraries(${PROJECT_NAME} Window)
else()
  message(FATAL_ERROR "Non-Windows platforms can only use NSDLDrv.")
endif()

if(TARGET_IS_ARM AND BUILD_FOR_PSVITA)
  set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
  # hammer in the PIE
  target_compile_options(${PROJECT_NAME} PRIVATE -fPIC -fPIE -pie)
  target_link_options(${PROJECT_NAME} PRIVATE -fPIC -fPIE -pie)
endif()

if(BUILD_STATIC)
  set(LINK_PACKAGES ${INSTALL_TARGETS})
  list(REMOVE_ITEM LINK_PACKAGES ${PROJECT_NAME})
  target_link_libraries(${PROJECT_NAME} -Wl,--whole-archive ${LINK_PACKAGES} -Wl,--no-whole-archive)
else()
  target_link_libraries(${PROJECT_NAME} Engine Core)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE UNREAL_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})
