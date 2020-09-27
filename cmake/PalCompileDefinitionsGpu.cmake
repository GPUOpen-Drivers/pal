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

pal_include_guard(PalCompilerDefinitionsGpu)

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
