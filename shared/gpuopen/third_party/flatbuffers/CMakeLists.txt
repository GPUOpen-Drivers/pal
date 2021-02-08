cmake_minimum_required(VERSION 3.10..3.16)

project(flatbuffers VERSION 1.12.0 LANGUAGES CXX)

add_library(flatbuffers INTERFACE)

target_include_directories(flatbuffers INTERFACE ${CMAKE_CURRENT_LIST_DIR})