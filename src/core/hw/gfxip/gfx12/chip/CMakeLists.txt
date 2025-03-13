##
 #######################################################################################################################
 #
 #  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

target_sources(pal PRIVATE
    CMakeLists.txt
    gfx12_merged_default.h
    gfx12_merged_enum.h
    gfx12_merged_mask.h
    gfx12_merged_offset.h
    gfx12_merged_registers.h
    gfx12_merged_shift.h
    gfx12_sq_ko_reg.h
    gfx12_merged_typedef.h
    gfx12_merged_pm4_it_opcodes.h
    gfx12_merged_f32_me_pm4_packets.h
    gfx12_merged_f32_mec_pm4_packets.h
    gfx12_merged_f32_pfp_pm4_packets.h
)
