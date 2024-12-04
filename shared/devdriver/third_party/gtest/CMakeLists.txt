# googletest appears to no longer publish releases. Intead they recommend to
# always get the latest commit in main branch.
#
# current commit: 5f467ec04df33024e3c6760fa403b5cd5d8e9ace
project(gtest LANGUAGES CXX)

add_library(gtest
    STATIC
        src/gtest-all.cc
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # Run-time type information (RTTI) support is disabled for GCC-like
    # compilers (see AMD.cmake). This causes build issues for Google Test
    # on these compilers, as `std::any` is not available without RTTI.
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_compile_options(gtest PUBLIC -frtti)
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_options(gtest PRIVATE -frtti)
    endif()
endif()

target_include_directories(gtest PUBLIC include)
target_include_directories(gtest PRIVATE .)
