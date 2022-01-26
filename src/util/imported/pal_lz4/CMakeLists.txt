### Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All rights reserved. ###

project(pal_lz4 LANGUAGES C)
add_library(pal_lz4 STATIC lz4.c lz4frame.c lz4hc.c xxhash.c)

target_compile_definitions(pal_lz4 INTERFACE LZ4_HC_STATIC_LINKING_ONLY)
target_include_directories(pal_lz4 INTERFACE .)
