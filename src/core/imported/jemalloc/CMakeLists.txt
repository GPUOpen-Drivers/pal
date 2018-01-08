project(JEMALLOC C)

### Create JEMALLOC Library ############################################################################################
add_library(jemalloc STATIC "")
install(TARGETS jemalloc DESTINATION ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}${TARGET_ARCHITECTURE_BITS})

### Cached Project Options #############################################################################################
option(JEMALLOC_STATS "Build ${PROJECT_NAME} with statistics support?" ${PAL_JEMALLOC_STATS})
option(JEMALLOC_DEBUG "Build ${PROJECT_NAME} in debug mode?" ${PAL_JEMALLOC_DEBUG})

### Compiler Options ###################################################################################################
if(WIN32)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        # CMAKE-TODO: These are /W4 (level 4) warnings
        # Review what is acceptable and what is not.
        target_compile_options(jemalloc PRIVATE
            /wd4057 # 'function' : 'X' differs in indirection to slightly different base types from 'Y'
            /wd4100 # unreferenced formal parameter
            /wd4127 # conditional expression is constant
            /wd4152 # nonstandard extension, function/data pointer conversion in expression
            /wd4189 # local variable is initialized but not referenced
            /wd4201 # nonstandard extension used : nameless struct/union
            /wd4244 # '=' : conversion from 'int' to 'unsigned char', possible loss of data
            /wd4565 # 'X' : redefinition; the symbol was previously declared with __declspec(restrict)
            /wd4701 # potentially uninitialized local variable
            /wd4702 # unreachable code
            /wd4706 # assignment within conditional expression

            # MSVC 1900 (VC 14)
            /wd4456 # declaration of 'X' hides previous local declaration
            /wd4457 # declaration of 'X' hides function parameter
            /wd4459 # declaration of 'X' hides global declaration
        )

        if(TARGET_ARCHETECTURE_BITS EQUAL 32)
            target_compile_options(jemalloc PRIVATE /wd4310) # cast truncates constant value
        endif()

        message(STATUS "Configured ${PROJECT_NAME} compiler options for MSVC.")
    else()
        message(FATAL_ERROR "Using unknown compiler")
    endif()
elseif(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # SEE: https://gcc.gnu.org/onlinedocs/gcc-6.2.0/gcc/Option-Summary.html#Option-Summary
        # for a list of all options and documentation.
        target_compile_options(jemalloc PRIVATE -pthread -fPIC)
        target_compile_options(jemalloc PRIVATE -Wno-unused-parameter -Wno-shift-negative-value -Wno-type-limits)

        message(STATUS "Configured ${PROJECT_NAME} compiler options for GCC.")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        #message(STATUS "Configured compiler options for Clang.")
        message(WARNING "Clang is untested.")
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()
endif()

### Defines/Includes/Sources ###########################################################################################
if(JEMALLOC_STATS)
    target_compile_definitions(jemalloc PRIVATE JEMALLOC_STATS)
endif()

target_include_directories(jemalloc
    PUBLIC
        ${PROJECT_SOURCE_DIR}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/msvc_compat
        ${PROJECT_SOURCE_DIR}/core
    )

target_sources(jemalloc PRIVATE
    ${PROJECT_SOURCE_DIR}/core/jemalloc.c
    ${PROJECT_SOURCE_DIR}/core/arena.c
    ${PROJECT_SOURCE_DIR}/core/base.c
    ${PROJECT_SOURCE_DIR}/core/bitmap.c
    ${PROJECT_SOURCE_DIR}/core/chunk.c
    ${PROJECT_SOURCE_DIR}/core/chunk_dss.c
    ${PROJECT_SOURCE_DIR}/core/chunk_mmap.c
    ${PROJECT_SOURCE_DIR}/core/ckh.c
    ${PROJECT_SOURCE_DIR}/core/ctl.c
    ${PROJECT_SOURCE_DIR}/core/extent.c
    ${PROJECT_SOURCE_DIR}/core/huge.c
    ${PROJECT_SOURCE_DIR}/core/mutex.c
    ${PROJECT_SOURCE_DIR}/core/nstime.c
    ${PROJECT_SOURCE_DIR}/core/pages.c
    ${PROJECT_SOURCE_DIR}/core/prof.c
    ${PROJECT_SOURCE_DIR}/core/quarantine.c
    ${PROJECT_SOURCE_DIR}/core/rtree.c
    ${PROJECT_SOURCE_DIR}/core/stats.c
    ${PROJECT_SOURCE_DIR}/core/tcache.c
    ${PROJECT_SOURCE_DIR}/core/tsd.c
    ${PROJECT_SOURCE_DIR}/core/util.c
    ${PROJECT_SOURCE_DIR}/core/witness.c
)

if (JEMALLOC_DEBUG)
    target_compile_definitions(jemalloc PRIVATE JEMALLOC_DEBUG)
    target_sources(jemalloc PRIVATE
        ${PROJECT_SOURCE_DIR}/core/ticker.c
        ${PROJECT_SOURCE_DIR}/core/prng.c
        ${PROJECT_SOURCE_DIR}/core/mb.c
        ${PROJECT_SOURCE_DIR}/core/hash.c
        ${PROJECT_SOURCE_DIR}/core/atomic.c
    )
endif()

# Enable valgrind: http://valgrind.org/
#    target_sources(jemalloc PRIVATE ${PROJECT_SOURCE_DIR}/core/valgrind.c)
# Enable zone allocator
#    target_sources(jemalloc PRIVATE ${PROJECT_SOURCE_DIR}/core/zone.c)

### Link Libraries #####################################################################################################
if(UNIX)
    target_link_libraries(jemalloc INTERFACE pthread)
endif()

### Visual Studio Filters ##############################################################################################
target_vs_filters(jemalloc)

### Mark all options as advanced #######################################################################################
if(JEMALLOC_OPTIONS_MARK_ADVANCED)
    mark_grouped_as_advanced(JEMALLOC)
endif()
