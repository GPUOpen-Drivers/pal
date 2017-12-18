cmake_minimum_required(VERSION 3.5)
project(PAL LANGUAGES C CXX)

### CMake Includes #####################################################################################################
include(CMakeDependentOption)
include(AmdCmakeHelper)

### Cached Project Options #############################################################################################
# CMAKE-TODO: To support multiple configurations for Visual Studio we can't use CMAKE_BUILD_TYPE
# We would have to override the options using cmake generator expressions and possibly include an
# option to disable overrides such as PAL_DISABLE_INTERNAL_OPTION_OVERRIDES

option(PAL_CLOSED_SOURCE "Build closed source PAL?" OFF)

option(PAL_BUILD_GPUOPEN "Build GPUOpen developer driver support?" OFF)

option(PAL_DBG_COMMAND_COMMENTS "Command with comments" OFF)

option(PAL_DEVELOPER_BUILD "Enable developer build" OFF)

option(PAL_ENABLE_PRINTS_ASSERTS "Enable print assertions?" ${CMAKE_BUILD_TYPE_DEBUG})
cmake_dependent_option(PAL_MEMTRACK "Enable PAL memory tracker?" ${CMAKE_BUILD_TYPE_DEBUG} "PAL_ENABLE_PRINTS_ASSERTS" OFF)

option(PAL_BUILD_JEMALLOC "Use jemalloc as the default PAL allocator?" ON)
cmake_dependent_option(PAL_JEMALLOC_STATS "Enable jemalloc statistice reporting?" OFF "PAL_BUILD_JEMALLOC" OFF)
cmake_dependent_option(PAL_JEMALLOC_DEBUG "Build jemalloc in debug mode?" OFF "PAL_BUILD_JEMALLOC" OFF)

option(PAL_BUILD_CORE "Build PAL Core?" ON)
option(PAL_BUILD_GPUUTIL "Build PAL GPU Util?" ON)
cmake_dependent_option(PAL_BUILD_LAYERS "Build PAL Layers?" ON "PAL_BUILD_GPUUTIL" OFF)

option(PAL_BUILD_DBG_OVERLAY "Build PAL Debug Overlay?" ON)

option(PAL_BUILD_GPU_PROFILER "Build PAL GPU Profiler?" ON)

option(PAL_BUILD_CMD_BUFFER_LOGGER "Build PAL Command Buffer Logger?" ${CMAKE_BUILD_TYPE_DEBUG})
option(PAL_BUILD_INTERFACE_LOGGER  "Build PAL Interface Logger?"      ${CMAKE_BUILD_TYPE_DEBUG})

option(PAL_BUILD_GFX  "Build PAL with Graphics support?" ON)
cmake_dependent_option(PAL_BUILD_GFX6 "Build PAL with GFX6 support?" ON "PAL_BUILD_GFX" OFF)
cmake_dependent_option(PAL_BUILD_GFX9 "Build PAL with GFX9 support?" ON "PAL_BUILD_GFX" OFF)

option(PAL_BUILD_OSS  "Build PAL with Operating System support?" ON)
cmake_dependent_option(PAL_BUILD_OSS1   "Build PAL with OSS1?"   ON "PAL_BUILD_OSS" OFF)
cmake_dependent_option(PAL_BUILD_OSS2   "Build PAL with OSS2?"   ON "PAL_BUILD_OSS" OFF)
cmake_dependent_option(PAL_BUILD_OSS2_4 "Build PAL with OSS2_4?" ON "PAL_BUILD_OSS" OFF)
cmake_dependent_option(PAL_BUILD_OSS4   "Build PAL with OSS4?"   ON "PAL_BUILD_OSS" OFF)

option(PAL_BUILD_DTIF "Build PAL with Driver-to-TCore2 InterFace support?" OFF)

# PAL Client Options ###############################################################################
# Use Vulkan as the default client.
set(PAL_CLIENT "VULKAN" CACHE STRING "Client interfacing with PAL.")

# Read the latest version from palLib.h
if(DEFINED PAL_CLIENT_INTERFACE_MAJOR_VERSION)
    message(STATUS "Client configured PAL_INTERFACE_MAJOR_VERSION as " ${PAL_CLIENT_INTERFACE_MAJOR_VERSION})
else()
    file(STRINGS inc/core/palLib.h PAL_MAJOR_VERSION REGEX "^#define PAL_INTERFACE_MAJOR_VERSION [0-9]+")

    if(PAL_MAJOR_VERSION STREQUAL "")
        message(STATUS "Failed to find PAL_INTERFACE_MAJOR_VERSION")
    else()
        string(REGEX REPLACE "^#define PAL_INTERFACE_MAJOR_VERSION " "" PAL_MAJOR_VERSION ${PAL_MAJOR_VERSION})
        message(STATUS "Detected PAL_INTERFACE_MAJOR_VERSION is " ${PAL_MAJOR_VERSION})
    endif()
    set(PAL_CLIENT_INTERFACE_MAJOR_VERSION ${PAL_MAJOR_VERSION})
endif()

# Option Overrides #################################################################################
# These are options that override PAL subproject options.  As these overrides are managed and force
# set by PAL, mark_as_advanced is used to hide them from the CMake GUI.

# ADDRLIB
set(PAL_ADDR_PATH ${PROJECT_SOURCE_DIR}/src/core/imported/addrlib CACHE PATH "Specify the path to the ADDRLIB project.")
set(ADDR_OPTIONS_MARK_ADVANCED ON)

set(ADDR_SI_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with SI support." FORCE)
set(ADDR_CI_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with CI support." FORCE)
set(ADDR_VI_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with VI support." FORCE)

set(ADDR_SI_CHIP_DIR ${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx6/chip CACHE PATH "PAL override for ADDRLIB SI/CI/VI register chip headers." FORCE)

if(PAL_BUILD_GFX9)
    set(ADDR_GFX9_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with GFX9 support." FORCE)
    set(ADDR_GFX9_CHIP_DIR ${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx9/chip CACHE PATH "PAL override for ADDRLIB GFX9 register chip headers." FORCE)
endif()

# JEMALLOC
if(PAL_BUILD_JEMALLOC)
    set(PAL_JEMALLOC_PATH ${PROJECT_SOURCE_DIR}/src/core/imported/jemalloc CACHE PATH "Specify the path to the jemalloc project.")
    set(JEMALLOC_OPTIONS_MARK_ADVANCED ON)

    set(JEMALLOC_STATS ${PAL_JEMALLOC_STATS} CACHE BOOL "PAL override." FORCE)
    set(JEMALLOC_DEBUG ${PAL_JEMALLOC_DEBUG} CACHE BOOL "PAL override." FORCE)
endif()

# METROHASH
set(PAL_METROHASH_PATH ${PROJECT_SOURCE_DIR}/src/core/imported/metrohash CACHE PATH "Specify the path to the MetroHash project.")

# VAM
set(PAL_VAM_PATH ${PROJECT_SOURCE_DIR}/src/core/imported/vam CACHE PATH "Specify the path to the VAM project.")

# GPUOPEN
if(PAL_BUILD_GPUOPEN)
    set(PAL_GPUOPEN_PATH ${PROJECT_SOURCE_DIR}/shared/gpuopen CACHE PATH "Specify the path to the GPUOPEN_PATH project.")
endif()

### Generator Dependencies #############################################################################################
find_package(PythonInterp 3)
if(NOT PYTHONINTERP_FOUND)
    if(UNIX)
        message(FATAL_ERROR "Python 3 is needed to generate some source files.")
    endif()
endif()
set(PYTHON_CMD ${PYTHON_EXECUTABLE})

find_package(Perl 5)
if(NOT PERL_FOUND)
    if(UNIX)
        message(FATAL_ERROR "Perl 5 is needed to generate some source files.")
    endif()
endif()
set(PERL_CMD ${PERL_EXECUTABLE})

### Add Subdirectories #################################################################################################
add_subdirectory(src)
