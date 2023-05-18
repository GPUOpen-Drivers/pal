##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################

#
# In cmake nomenclature, a compile definition (ifdef) or similar setting is considered 'PUBLIC' if it needs to be seen
# by clients using PAL's headers and 'PRIVATE' if it does not. Keeping more ifdefs PRIVATE generally helps to do dirty
# builds faster and makes the compile command lines easier to debug. PUBLIC defines will automatically be propogated to
# things that depend on PAL (with `target_link_libraries`) and requires no additional action on the clients' part. In
# fact, the client should *never* explicitly add any PAL ifdefs because they may not match what PAL was built with.
#
# The gist for all of the following: if it's used anywhere in inc/*, it should be PUBLIC, otherwise make it PRIVATE.
#
# NOTE:
#   PAL's coding standard prefers the use of "#if" construct instead of the "#ifdef" construct
#   This means when making a new compile definition you should assign it a value
#
# EX:
#   target_compile_definitions(pal PRIVATE PAL_FOO=1)
include_guard()

function(pal_compile_definitions_gfx6 TARGET)
    # Needs to be public.
    # See the following directories:
    #   inc\gpuUtil\cas\...
    #   inc\gpuUtil\mlaa\...
    #   inc\gpuUtil\textWriter\...
    #   inc\gpuUtil\timeGraphics\...
    target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_GFX6=1)

endfunction()

function(pal_compile_definitions_gfx9 TARGET)
    target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_GFX9=1)

    target_compile_definitions(${TARGET} INTERFACE PAL_BUILD_GFX10=1)

    if(PAL_BUILD_NAVI12)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_NAVI12=1)
        target_compile_definitions(${TARGET} PRIVATE CHIP_HDR_NAVI12=1)
    endif()

    if(PAL_BUILD_NAVI14)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_NAVI14=1)
        target_compile_definitions(${TARGET} PRIVATE CHIP_HDR_NAVI14=1)
    endif()

    if(PAL_BUILD_NAVI21)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_NAVI21=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_GFX103=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_NAVI2X=1)
        target_compile_definitions(${TARGET} PRIVATE CHIP_HDR_NAVI21=1)
    endif()

    if(PAL_BUILD_NAVI22)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_NAVI22=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_GFX103=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_NAVI2X=1)
        target_compile_definitions(${TARGET} PRIVATE CHIP_HDR_NAVI22=1)
    endif()

    if(PAL_BUILD_NAVI23)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_NAVI23=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_GFX103=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_NAVI2X=1)
        target_compile_definitions(${TARGET} PRIVATE CHIP_HDR_NAVI23=1)
    endif()

    if(PAL_BUILD_NAVI24)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_NAVI24=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_GFX103=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_NAVI2X=1)
        target_compile_definitions(${TARGET} PRIVATE CHIP_HDR_NAVI24=1)
    endif()

    if(PAL_BUILD_REMBRANDT)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_GFX103=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_REMBRANDT=1)
        target_compile_definitions(${TARGET} PRIVATE CHIP_HDR_REMBRANDT=1)
    endif()

    if(PAL_BUILD_RAPHAEL)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_GFX103=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_RAPHAEL=1)
        target_compile_definitions(${TARGET} PRIVATE CHIP_HDR_RAPHAEL=1)
    endif()

    if(PAL_BUILD_MENDOCINO)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_GFX103=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_MENDOCINO=1)
        target_compile_definitions(${TARGET} PRIVATE CHIP_HDR_RAPHAEL=1)
    endif()

#if PAL_BUILD_NAVI31
    if(PAL_BUILD_NAVI31)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_NAVI31=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_NAVI3X=1)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_GFX11=1)
        target_compile_definitions(${TARGET} PRIVATE CHIP_HDR_NAVI31=1)
    endif()
#endif

endfunction()

function(pal_compile_definitions_gpu TARGET)
    if (PAL_BUILD_CORE AND PAL_BUILD_GFX)
        target_compile_definitions(${TARGET} PRIVATE PAL_BUILD_GFX=1)
        if(PAL_BUILD_GFX6)
            pal_compile_definitions_gfx6(${TARGET})
        endif()
        if(PAL_BUILD_GFX9)
            pal_compile_definitions_gfx9(${TARGET})
        endif()

    endif()
endfunction()

function(pal_compile_definitions TARGET)
    target_compile_definitions(${TARGET} PUBLIC
        PAL_CLIENT_INTERFACE_MAJOR_VERSION=${PAL_CLIENT_INTERFACE_MAJOR_VERSION}

        # Both of these macros are used to describe debug builds
        # TODO: Pal should only use one of these.
        $<$<CONFIG:Debug>:
            PAL_DEBUG_BUILD=1
            DEBUG=1
        >

    )

    if (PAL_CLIENT_VULKAN)
        target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_SUPPORT_DEPTHCLAMPMODE_ZERO_TO_ONE=1)
    endif()

    pal_get_cpu_endianness(TARGET_ARCHITECTURE_ENDIANESS)
    pal_get_system_architecture_bits(TARGET_ARCHITECTURE_BITS)

    target_compile_definitions(${TARGET} PRIVATE
        # Useful for determining determining the architecture (32 vs 64)
        PAL_COMPILE_TYPE=${TARGET_ARCHITECTURE_BITS}

        # Ex: BIGENDIAN_CPU or LITTLEENDIAN_CPU
        ${TARGET_ARCHITECTURE_ENDIANESS}ENDIAN_CPU=1
    )

    # If this build is part of a release branch, define the variable
    if (PAL_BUILD_BRANCH)
        target_compile_definitions(${TARGET} PRIVATE PAL_BUILD_BRANCH=${PAL_BUILD_BRANCH})
    endif()

    if (PAL_BUILD_GPUUTIL)
        target_compile_definitions(${TARGET} PRIVATE PAL_BUILD_GPUUTIL=1)
    endif()

    if(PAL_BUILD_NULL_DEVICE)
        target_compile_definitions(${TARGET} PRIVATE PAL_BUILD_NULL_DEVICE=1)
    endif()

    if(PAL_ENABLE_PRINTS_ASSERTS)
        target_compile_definitions(${TARGET} PUBLIC
            $<$<NOT:$<CONFIG:Debug>>:PAL_ENABLE_PRINTS_ASSERTS=1>
        )
    endif()

    if(PAL_ENABLE_PRINTS_ASSERTS_DEBUG)
        target_compile_definitions(${TARGET} PUBLIC
            $<$<CONFIG:Debug>:PAL_ENABLE_PRINTS_ASSERTS=1>
        )
    endif()

    if(PAL_ENABLE_LOGGING)
        target_compile_definitions(${TARGET} PUBLIC
            $<$<CONFIG:Debug>:PAL_ENABLE_LOGGING=1>
        )
    endif()

    target_compile_definitions(${TARGET} PUBLIC
        # Turn on memory tracking in Debug builds or when the user asks for it
        $<$<OR:$<CONFIG:Debug>,$<BOOL:${PAL_MEMTRACK}>>:
            PAL_MEMTRACK=1
        >
    )

    if(PAL_AMDGPU_BUILD AND PAL_DISPLAY_DCC)
        target_compile_definitions(${TARGET} PRIVATE PAL_DISPLAY_DCC=1)
    endif()

#if PAL_DEVELOPER_BUILD
    if(PAL_DEVELOPER_BUILD)
        # Enable default developer features, needs to be public since it's used in interface files.
        target_compile_definitions(${TARGET} PUBLIC PAL_DEVELOPER_BUILD=1)
    endif()
#endif

    # Describe the client
    target_compile_definitions(${TARGET} PUBLIC PAL_CLIENT_${PAL_CLIENT}=1)

    # This will be defined if we should get KMD headers from https://github.com/microsoft/libdxg
    # instead of the WDK
    if(PAL_USE_LIBDXG)
        target_compile_definitions(${TARGET} PUBLIC PAL_USE_LIBDXG=1)
    endif()

    if (PAL_BUILD_CORE)
        target_compile_definitions(${TARGET} PRIVATE PAL_BUILD_CORE=1)
    endif()

    if(PAL_AMDGPU_BUILD)
        message(STATUS "PAL build with amdgpu back-end enabled")

        target_compile_definitions(${TARGET} PUBLIC PAL_AMDGPU_BUILD=1)

        if(PAL_BUILD_DRI3)
            target_compile_definitions(${TARGET} PRIVATE PAL_HAVE_DRI3_PLATFORM=1)
        endif()

        if (PAL_BUILD_WAYLAND)
            target_compile_definitions(${TARGET} PRIVATE PAL_HAVE_WAYLAND_PLATFORM=1)
        endif()
    endif()

    if (PAL_BUILD_OSS)
        target_compile_definitions(${TARGET} PRIVATE PAL_BUILD_OSS=1)

#if PAL_BUILD_OSS2_4
        if(PAL_BUILD_OSS2_4)
            target_compile_definitions(${TARGET} PUBLIC PAL_BUILD_OSS2_4=1)
        endif()
#endif

        if(PAL_BUILD_OSS4)
            target_compile_definitions(${TARGET} PRIVATE PAL_BUILD_OSS4=1)
        endif()
    endif()

    if (PAL_64BIT_ARCHIVE_FILE_FMT)
        target_compile_definitions(${TARGET} PUBLIC PAL_64BIT_ARCHIVE_FILE_FMT=1)
    endif()

    pal_compile_definitions_gpu(${TARGET})

endfunction()
