# googletest appears to no longer publish releases. Intead they recommend to
# always get the latest commit in main branch.
#
# current commit: 5f467ec04df33024e3c6760fa403b5cd5d8e9ace
project(gtest LANGUAGES CXX)

add_library(gtest
    STATIC
        src/gtest-all.cc
)

target_include_directories(gtest PUBLIC include)
target_include_directories(gtest PRIVATE .)
