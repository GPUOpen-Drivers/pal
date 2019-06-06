
cmake_minimum_required(VERSION 3.5)

function(amd_target name)

#if DD_CLOSED_SOURCE
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
#endif
        # [GCC] Exceptions
        #   https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_exceptions.html
        #
        # [GCC] Options Controlling C++ Dialect
        #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/C_002b_002b-Dialect-Options.html
        #
        # [GCC] Options That Control Optimization
        #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/Optimize-Options.html
        target_compile_options(${name} PRIVATE
            -fno-exceptions  # Disable exception handling support.
            -fno-rtti        # Disable run-time type information support.
            -fno-math-errno) # Single instruction math operations do not set ERRNO.

        # [GCC] Options to Request or Suppress Warnings
        #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/Warning-Options.html
        target_compile_options(${name} PRIVATE
            -Wall    # Enable warnings about questionable language constructs.
            -Wextra  # Enable extra warnings that are not enabled by -Wall.
            -Werror) # Turn warnings into errors.

#if DD_CLOSED_SOURCE
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

        # [MSVC] Warning Level
        #    https://docs.microsoft.com/en-us/cpp/build/reference/compiler-option-warning-level
        target_compile_options(${name} PRIVATE
            /W4  # Enable warning level 4.
            /WX) # Treat warnings as errors.

    else()
        message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported!")
    endif()
#endif

endfunction()

function(amd_executable name)

    add_executable(${name} ${ARGN} "")
    amd_target    (${name})

endfunction()

#if DD_CLOSED_SOURCE
function(amd_km_library name type)

    # TODO: Explain that this is temporary
    if(NOT DEFINED WDK_ROOT)
        find_package(WDK REQUIRED)
    endif()

    wdk_add_library(${name} ${type} ${ARGN} "")
    amd_target (${name})

endfunction()
#endif

function(amd_um_library name type)

    add_library(${name} ${type} ${ARGN} "")
    amd_target (${name})

    set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)

endfunction()

function(amd_library name type)

    amd_um_library(${name} ${type} ${ARGN} "")

endfunction()
