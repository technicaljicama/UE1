project(Core CXX)

set(SRC_FILES
  "Src/UnCache.cpp"
  "Src/UnClass.cpp"
  "Src/UnCorSc.cpp"
  "Src/UnFile.cpp"
  "Src/UnMem.cpp"
  "Src/UnName.cpp"
  "Src/UnObj.cpp"
  "Src/UnOutDev.cpp"
  "Src/UnPlat.cpp"
  "Src/UnProp.cpp"
  "Src/Core.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE CORE_EXPORTS)
