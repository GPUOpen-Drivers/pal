project(CWPACK C)

### Create CWPACK Library ############################################################################################
add_library(cwpack STATIC "")
install(TARGETS cwpack DESTINATION ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}${TARGET_ARCHITECTURE_BITS})

### Cached Project Options #############################################################################################
# No exposed CWPACK options

### Compiler Options ###################################################################################################
if(WIN32)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        message(STATUS "Configured ${PROJECT_NAME} compiler options for MSVC.")
    else()
        message(FATAL_ERROR "Using unknown compiler")
    endif()
elseif(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # SEE: https://gcc.gnu.org/onlinedocs/gcc-6.2.0/gcc/Option-Summary.html#Option-Summary
        # for a list of all options and documentation.
        target_compile_options(cwpack PRIVATE -pthread -fPIC)

        message(STATUS "Configured ${PROJECT_NAME} compiler options for GCC.")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        message(WARNING "Clang is untested.")
        target_compile_options(cwpack PRIVATE -pthread -fPIC)
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()
endif()

### Defines/Includes/Sources ###########################################################################################
target_compile_definitions(cwpack PRIVATE COMPILE_FOR_${TARGET_ARCHITECTURE_ENDIANESS}_ENDIAN)

if(WIN32)
    if(TARGET_ARCHITECTURE_BITS EQUAL 32)
        target_compile_definitions(cwpack PRIVATE CWP_CALL=__fastcall)
    endif()
endif()

target_include_directories(cwpack
    PUBLIC
        ${PROJECT_SOURCE_DIR}/inc
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
    )

target_sources(cwpack PRIVATE
    ${PROJECT_SOURCE_DIR}/src/cwpack.c
)

### Link Libraries #####################################################################################################
# No external libraries required for CWPACK.

### Find headers and build source groups ###############################################################################
target_find_headers(cwpack)
target_source_groups(cwpack)

### Mark all options as advanced #######################################################################################
if(CWPACK_OPTIONS_MARK_ADVANCED)
    mark_grouped_as_advanced(CWPACK)
endif()
