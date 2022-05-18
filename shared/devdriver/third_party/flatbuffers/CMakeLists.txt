project(flatbuffers VERSION 1.12.0 LANGUAGES CXX)

add_library(flatbuffers INTERFACE)

target_include_directories(flatbuffers INTERFACE ${CMAKE_CURRENT_LIST_DIR})