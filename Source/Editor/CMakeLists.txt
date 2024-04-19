project(Editor CXX)

if(NOT TARGET_IS_WINDOWS)
  message(FATAL_ERROR "Editor is for Windows only.")
endif()

set(SRC_FILES
  "Src/CoolBsp.cpp"
  "Src/UnBsp.cpp"
  "Src/UnEdAct.cpp"
  "Src/UnEdCam.cpp"
  "Src/UnEdClick.cpp"
  "Src/UnEdCnst.cpp"
  "Src/UnEdCsg.cpp"
  "Src/UnEdFact.cpp"
  "Src/UnEdRend.cpp"
  "Src/UnEdSrv.cpp"
  "Src/UnEdTran.cpp"
  "Src/UnEditor.cpp"
  "Src/UnMeshEd.cpp"
  "Src/UnScrCom.cpp"
  "Src/UnShadow.cpp"
  "Src/UnSyntax.cpp"
  "Src/UnTopics.cpp"
  "Src/UnVisi.cpp"
  "Src/EdHook.cpp"
  "Src/Editor.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_link_libraries(${PROJECT_NAME} Window Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE EDITOR_EXPORTS)
