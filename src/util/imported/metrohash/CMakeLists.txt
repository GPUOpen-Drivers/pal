project(METROHASH CXX)

### Create METROHASH Library ############################################################################################
add_library(metrohash STATIC "")
install(TARGETS metrohash DESTINATION ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}${TARGET_ARCHITECTURE_BITS})

### Cached Project Options #############################################################################################
# No exposed METROHASH options

### Compiler Options ###################################################################################################
if(WIN32)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        # CMAKE-TODO: These are /W4 (level 4) warnings
        # Review what is acceptable and what is not.
        # target_compile_options(metrohash PRIVATE
        #     /wd4057
        # )

        message(STATUS "Configured ${PROJECT_NAME} compiler options for MSVC.")
    else()
        message(FATAL_ERROR "Using unknown compiler")
    endif()
elseif(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # SEE: https://gcc.gnu.org/onlinedocs/gcc-6.2.0/gcc/Option-Summary.html#Option-Summary
        # for a list of all options and documentation.
        target_compile_options(metrohash PRIVATE -pthread -fPIC)
        # target_compile_options(metrohash PRIVATE -Wno-unused-parameter -Wno-shift-negative-value -Wno-type-limits)

        message(STATUS "Configured ${PROJECT_NAME} compiler options for GCC.")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        #message(STATUS "Configured compiler options for Clang.")
        message(WARNING "Clang is untested.")
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()
endif()

### Defines/Includes/Sources ###########################################################################################
target_include_directories(metrohash PUBLIC ${PROJECT_SOURCE_DIR}/src)

target_sources(metrohash PRIVATE
    ${PROJECT_SOURCE_DIR}/src/metrohash64.cpp
    ${PROJECT_SOURCE_DIR}/src/metrohash128.cpp
)

### Link Libraries #####################################################################################################
# if(UNIX)
#     target_link_libraries(metrohash INTERFACE pthread)
# endif()

### Find headers and build source groups ###############################################################################
target_find_headers(metrohash)
target_source_groups(metrohash)

### Mark all options as advanced #######################################################################################
if(METROHASH_OPTIONS_MARK_ADVANCED)
    mark_grouped_as_advanced(METROHASH)
endif()
