cmake_minimum_required(VERSION 3.12)

project("glad")

# Files
add_library(${PROJECT_NAME} INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include/KHR/khrplatform.h
  ${CMAKE_CURRENT_LIST_DIR}/include/glad/glad.h
  ${CMAKE_CURRENT_LIST_DIR}/src/glad.c
)

# Set output name
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

# Properties

set_target_properties(${PROJECT_NAME} PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}/Vendor/${PROJECT_NAME}"
  LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}/Vendor/${PROJECT_NAME}"
  RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}/Vendor/${PROJECT_NAME}"
)

# Include dirs

target_include_directories(${PROJECT_NAME} INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Compile definitions

target_compile_options(${PROJECT_NAME} INTERFACE
  $<$<COMPILE_LANG_AND_ID:C,MSVC>:/MP>
  $<$<COMPILE_LANG_AND_ID:C,MSVC>:/MD>
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/MP>
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/MD>
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/EHsc>
)
