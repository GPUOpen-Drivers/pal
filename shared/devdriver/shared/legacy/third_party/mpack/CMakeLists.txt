project(MPack VERSION 1.0.0 LANGUAGES CXX)

add_library(mpack STATIC
    mpack.h
    mpack.cpp
)

target_include_directories(mpack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# We manually patch the generated message pack blob when writing objects or arrays.
# This confuses the debug write tracking, so we have turn it off for our hack to work.
# This is marked public because all uses of the header (gpuopen) need to use it.
target_compile_definitions(mpack PUBLIC "MPACK_WRITE_TRACKING=0")

if (WIN32)
    target_compile_definitions(mpack
        PUBLIC
            "_CRT_INSECURE_NO_DEPRECATE"
            "_CRT_SECURE_NO_WARNINGS"
    )
endif()
