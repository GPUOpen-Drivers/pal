##
 #######################################################################################################################
 #
 #  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
include(PalVersionHelper)

pal_include_guard(PalCompilerDefinitions)

function(pal_compile_definitions_gfx6)
    # Needs to be public.
    # See the following directories:
    #   inc\gpuUtil\cas\...
    #   inc\gpuUtil\mlaa\...
    #   inc\gpuUtil\textWriter\...
    #   inc\gpuUtil\timeGraphics\...
    target_compile_definitions(pal PUBLIC PAL_BUILD_GFX6=1)

endfunction()

function(pal_compile_definitions_gfx9)
    target_compile_definitions(pal PUBLIC PAL_BUILD_GFX9=1)

    target_compile_definitions(pal PUBLIC PAL_BUILD_GFX10=1)

    if(PAL_BUILD_NAVI14)
        target_compile_definitions(pal PUBLIC PAL_BUILD_NAVI14=1)
        target_compile_definitions(pal PRIVATE CHIP_HDR_NAVI14=1)
    endif()

endfunction()

function(pal_compile_definitions_gpu)
    if (PAL_BUILD_CORE AND PAL_BUILD_GFX)
        target_compile_definitions(pal PRIVATE PAL_BUILD_GFX=1)

        if(PAL_BUILD_GFX6)
            pal_compile_definitions_gfx6()
        endif()

        if(PAL_BUILD_GFX9)
            pal_compile_definitions_gfx9()
        endif()
    endif()
endfunction()

function(pal_compile_definitions)
    target_compile_definitions(pal PUBLIC
        PAL_CLIENT_INTERFACE_MAJOR_VERSION=${PAL_CLIENT_INTERFACE_MAJOR_VERSION}

        # Both of these macros are used to describe debug builds
        # TODO: Pal should only use one of these.
        $<$<CONFIG:Debug>:
            PAL_DEBUG_BUILD=1
            DEBUG=1
        >

    )

    target_compile_definitions(pal PRIVATE
        # Useful for determining determining the architecture (32 vs 64)
        PAL_COMPILE_TYPE=${TARGET_ARCHITECTURE_BITS}

        # Ex: BIGENDIAN_CPU or LITTLEENDIAN_CPU
        ${TARGET_ARCHITECTURE_ENDIANESS}ENDIAN_CPU=1
    )

    # If this build is part of a release branch, define the variable
    if (DEFINED PAL_BUILD_BRANCH)
        target_compile_definitions(pal PRIVATE PAL_BUILD_BRANCH=${PAL_BUILD_BRANCH})
    endif()

    if (PAL_BUILD_GPUUTIL)
        target_compile_definitions(pal PRIVATE PAL_BUILD_GPUUTIL=1)
    endif()

    if(PAL_BUILD_NULL_DEVICE)
        target_compile_definitions(pal PRIVATE PAL_BUILD_NULL_DEVICE=1)
    endif()

    if(PAL_BUILD_GPUOPEN)
        target_compile_definitions(pal PUBLIC PAL_BUILD_GPUOPEN=1)
    endif()

    if(PAL_ENABLE_DEVDRIVER_USAGE)
        target_compile_definitions(pal PRIVATE PAL_ENABLE_DEVDRIVER_USAGE=1)
    endif()

    if(PAL_ENABLE_PRINTS_ASSERTS)
        target_compile_definitions(pal PUBLIC
            $<$<NOT:$<CONFIG:Debug>>:PAL_ENABLE_PRINTS_ASSERTS=1>
        )
    endif()

    if(PAL_ENABLE_PRINTS_ASSERTS_DEBUG)
        target_compile_definitions(pal PUBLIC
            $<$<CONFIG:Debug>:PAL_ENABLE_PRINTS_ASSERTS=1>
        )
    endif()

    target_compile_definitions(pal PUBLIC
        # Turn on memory tracking in Debug builds or when the user asks for it
        $<$<OR:$<CONFIG:Debug>,$<BOOL:${PAL_MEMTRACK}>>:
            PAL_MEMTRACK=1
        >
    )

    if(UNIX)
        if (PAL_DISPLAY_DCC)
            target_compile_definitions(pal PRIVATE PAL_DISPLAY_DCC=1)
        endif()
    endif()

#if PAL_DEVELOPER_BUILD
    if(PAL_DEVELOPER_BUILD)
        target_compile_definitions(pal PUBLIC PAL_DEVELOPER_BUILD=1)
    endif()
#endif

    if(PAL_DBG_COMMAND_COMMENTS)
        target_compile_definitions(pal PRIVATE PAL_DBG_COMMAND_COMMENTS=1)
    endif()

    # Describe the client
    target_compile_definitions(pal PUBLIC PAL_CLIENT_${PAL_CLIENT}=1)

    target_compile_definitions(pal PRIVATE PAL_BUILD_CORE=1)

    if(PAL_AMDGPU_BUILD)
        message_verbose("PAL build with amdgpu back-end enabled")

        target_compile_definitions(pal PUBLIC PAL_AMDGPU_BUILD=1)

        if(PAL_BUILD_DRI3)
            message_verbose("PAL build with DRI3 enabled")

            target_compile_definitions(pal PRIVATE PAL_HAVE_DRI3_PLATFORM=1)
        endif()

        if (PAL_BUILD_WAYLAND)
            message_verbose("PAL build with Wayland enabled")

            target_compile_definitions(pal PRIVATE PAL_HAVE_WAYLAND_PLATFORM=1)
        endif()
    endif()

    if (PAL_BUILD_OSS)
        target_compile_definitions(pal PRIVATE PAL_BUILD_OSS=1)

        if(PAL_BUILD_OSS1)
            target_compile_definitions(pal PRIVATE PAL_BUILD_OSS1=1)
        endif()

        if(PAL_BUILD_OSS2)
            target_compile_definitions(pal PRIVATE PAL_BUILD_OSS2=1)
        endif()

        if(PAL_BUILD_OSS2_4)
            target_compile_definitions(pal PRIVATE PAL_BUILD_OSS2_4=1)
        endif()

        if(PAL_BUILD_OSS4)
            target_compile_definitions(pal PRIVATE PAL_BUILD_OSS4=1)
        endif()
    endif()

    if(PAL_BUILD_LAYERS)
        target_compile_definitions(pal PRIVATE PAL_BUILD_LAYERS=1)

        if(PAL_BUILD_DBG_OVERLAY)
            target_compile_definitions(pal PRIVATE PAL_BUILD_DBG_OVERLAY=1)
        endif()

        if(PAL_BUILD_GPU_PROFILER)
            target_compile_definitions(pal PRIVATE PAL_BUILD_GPU_PROFILER=1)
        endif()

        # Enable cmd buffer logging on debug configs or when the client asks for it
        target_compile_definitions(pal PRIVATE
            $<$<OR:$<CONFIG:Debug>,$<BOOL:${PAL_BUILD_CMD_BUFFER_LOGGER}>>:
                PAL_BUILD_CMD_BUFFER_LOGGER=1
            >
        )

        # Enable interface logging on debug configs or when the client asks for it
        target_compile_definitions(pal PRIVATE
            $<$<OR:$<CONFIG:Debug>,$<BOOL:${PAL_BUILD_INTERFACE_LOGGER}>>:
                PAL_BUILD_INTERFACE_LOGGER=1
            >
        )

        # Enable pm4 instrumentor on debug configs or when the client asks for it
        # This needs to be public, see inc/core/palDeveloperHooks.h
        target_compile_definitions(pal PUBLIC
            $<$<OR:$<CONFIG:Debug>,$<BOOL:${PAL_BUILD_PM4_INSTRUMENTOR}>>:
                PAL_BUILD_PM4_INSTRUMENTOR=1
            >
        )
    endif()

    pal_compile_definitions_gpu()

endfunction()
