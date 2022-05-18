project(lz4 VERSION 1.9.1 LANGUAGES C)

add_library(xxhash STATIC xxhash.h xxhash.c)

add_library(lz4 STATIC lz4.h lz4hc.h lz4frame.h lz4_all.c)

target_link_libraries(lz4 PUBLIC xxhash)

target_compile_definitions(lz4 PUBLIC LZ4_DISABLE_DEPRECATE_WARNINGS)
