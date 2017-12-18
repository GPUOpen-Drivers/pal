#
# Trade secret of Advanced Micro Devices, Inc.
# Copyright 2017, Advanced Micro Devices, Inc., (unpublished)
#
# All rights reserved.  This notice is intended as a precaution against inadvertent publication and does not imply
# publication or any waiver of confidentiality.  The year included in the foregoing notice is the year of creation of
# the work.
#

### Create VAM Library #################################################################################################
project(VAM C CXX)

add_library(vam STATIC "")
install(TARGETS vam DESTINATION ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}${TARGET_ARCHITECTURE_BITS})

### Cached Project Options #############################################################################################
# No exposed VAM options

# CMAKE-TODO: As there is some difficulty specifying multiple libs to link to with the AMD make system
# embedded builds will be used instead.
# ${PROJECT_SOURCE_DIR}/ also has to be added in front of the sources.
set(VAM_SCOPE PRIVATE)
if(AMD_MAKE_BUILD)
    option(VAM_EMBEDDED "Build ${PROJECT_NAME} embedded into parent project?" ON)
    if(VAM_EMBEDDED)
        set(VAM_SCOPE PUBLIC)
    endif()
endif()

### Compiler Options ###################################################################################################
if(WIN32)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        # CMAKE-TODO: These are /W4 (level 4) warnings
        # Review what is acceptable and what is not.
        target_compile_options(vam ${VAM_SCOPE}
            /wd4100 # unreferenced formal parameter
            /wd4127 # conditional expression is constant
            /wd4189 # local variable is initialized but not referenced
            /wd4201 # nonstandard extension used : nameless struct/union
            /wd4701 # potentially uninitialized local variable
            /wd4703 # potentially uninitialized local pointer variable
        )

        message(STATUS "Configured ${PROJECT_NAME} compiler options for MSVC.")
    else()
        message(FATAL_ERROR "Using unknown compiler")
    endif()
elseif(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # SEE: https://gcc.gnu.org/onlinedocs/gcc-6.2.0/gcc/Option-Summary.html#Option-Summary
        # for a list of all options and documentation.
        target_compile_options(vam ${VAM_SCOPE} $<$<COMPILE_LANGUAGE:CXX>:-fPIC -fcheck-new -fno-rtti -fno-math-errno -std=c++0x>)
        target_compile_options(vam ${VAM_SCOPE} $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused -Wno-ignored-qualifiers -Wno-missing-field-initializers>)

        message(STATUS "Configured ${PROJECT_NAME} compiler options for GCC.")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        #message(STATUS "Configured compiler options for Clang.")
        message(WARNING "Clang is untested.")
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()
endif()

### Include Directories ################################################################################################
target_include_directories(vam
    PUBLIC
        ${PROJECT_SOURCE_DIR}
    ${VAM_SCOPE}
        ${PROJECT_SOURCE_DIR}/core
        ${GLOBAL_ROOT_SRC_DIR}/drivers/inc/shared
        ${GLOBAL_ROOT_SRC_DIR}/drivers/inc/asic_reg
)

### VAM Sources ########################################################################################################
target_sources(vam ${VAM_SCOPE}
    ${PROJECT_SOURCE_DIR}/vaminterface.cpp
    ${PROJECT_SOURCE_DIR}/core/vamobject.cpp
    ${PROJECT_SOURCE_DIR}/core/vamdevice.cpp
    ${PROJECT_SOURCE_DIR}/core/vamptb.cpp
    ${PROJECT_SOURCE_DIR}/core/vamsuballoc.cpp
    ${PROJECT_SOURCE_DIR}/core/vamsectionalloc.cpp
    ${PROJECT_SOURCE_DIR}/core/vamrange.cpp
)

### Visual Studio Filters ##############################################################################################
target_vs_filters(vam)
