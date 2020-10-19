cmake_minimum_required(VERSION 3.10..3.16)
project(RapidJSON VERSION 1.1.0 LANGUAGES CXX)

add_library(rapidjson INTERFACE)
target_include_directories(rapidjson INTERFACE "include")

# C++ STL defines an interface for "iterator" objects. It also supplies a class,
# std::iterator that includes all of these methods to implement. This has been
# deprecated in C++17, in favor of using templates instead of inheritance.
# We disable this because we don't want to modify third party code if it can be helped.
#
# Because rapidjson is a header-only library, we cannot silence warnings in it
# Any settings here bleed into code that uses rapdijson. :(
#
# See this blog for more details about these warnings:
#       https://mariusbancila.ro/blog/2018/07/09/visual-studio-warning-macros-for-c17-deprecated-features/
target_compile_definitions(rapidjson
    INTERFACE
        " _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING"
)
