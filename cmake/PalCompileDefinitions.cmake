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
include(PalCompileDefinitionsGpu)

pal_include_guard(PalCompilerDefinitions)

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
function(pal_compile_definitions)
    target_compile_definitions(pal PUBLIC
        PAL_CLIENT_INTERFACE_MAJOR_VERSION=${PAL_CLIENT_INTERFACE_MAJOR_VERSION}
        PAL_CLIENT_INTERFACE_MINOR_VERSION=${PAL_CLIENT_INTERFACE_MINOR_VERSION}

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
        target_compile_definitions(pal PUBLIC PAL_AMDGPU_BUILD=1)

        if(PAL_BUILD_DRI3)
            target_compile_definitions(pal PRIVATE PAL_HAVE_DRI3_PLATFORM=1)
        endif()

        if (PAL_BUILD_WAYLAND)
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
        target_compile_definitions(pal PRIVATE
            $<$<OR:$<CONFIG:Debug>,$<BOOL:${PAL_BUILD_PM4_INSTRUMENTOR}>>:
                PAL_BUILD_PM4_INSTRUMENTOR=1
            >
        )
    endif()

    pal_compile_definitions_gpu()

endfunction()
