cmake_minimum_required(VERSION 3.5)
project(RapidJSON VERSION 1.1.0 LANGUAGES CXX)

add_library(rapidjson INTERFACE)
target_include_directories(rapidjson INTERFACE "include")
