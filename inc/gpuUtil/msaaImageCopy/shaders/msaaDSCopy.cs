/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

### Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved. ###
[CsIl]
il_cs_2_0
dcl_global_flags refactoringAllowed
; UAV0 - DST MSAA Image 1/2/4/8x
; UAV1 - SRC MSAA Image 1/2/4/8x
dcl_typed_uav_id(0)_type(2dmsaa)_fmtx(uint)
dcl_typed_uav_id(1)_type(2dmsaa)_fmtx(uint)

; cb0[0] = (source X offset, source Y offset, copy width, copy height)
; cb0[1] = (dest X offset, dest Y offset, src sample count, dst sample count)
dcl_cb cb0[2]

dcl_num_thread_per_group 8, 8, 1

dcl_literal l0, 0, 1, 2, 3
dcl_literal l1, 4, 5, 6, 7

; Check if the absolute thread ID is outside the bounds of the image copy extents
ult  r0.__zw, vAbsTid0.xxxy, cb0[0].zzzw
iand r0.__z_, r0.w, r0.z
if_logicalnz r0.z

    ; Add source copy offsets to the thread ID to compute texture location.
    iadd r0.xyz, vAbsTid0.xyz, cb0[0].xy0

    ; Add dest copy offsets to the thread ID to compute texture location.
    iadd r9.xyz, vAbsTid0.xyz, cb0[1].xy0

    switch cb0[1].z
    case 1

        mov r0.w, l0.x
        uav_load_id(1) r1, r0

        switch cb0[1].w
        case 2

        mov r9.w, l0.x
        uav_store_id(0) r9, r1

        mov r9.w, l0.y
        uav_store_id(0) r9, r1
        break

        case 4

        mov r9.w, l0.x
        uav_store_id(0) r9, r1

        mov r9.w, l0.y
        uav_store_id(0) r9, r1

        mov r9.w, l0.z
        uav_store_id(0) r9, r1

        mov r9.w, l0.w
        uav_store_id(0) r9, r1
        break

        case 8

        mov r9.w, l0.x
        uav_store_id(0) r9, r1

        mov r9.w, l0.y
        uav_store_id(0) r9, r1

        mov r9.w, l0.z
        uav_store_id(0) r9, r1

        mov r9.w, l0.w
        uav_store_id(0) r9, r1

        mov r9.w, l1.x
        uav_store_id(0) r9, r1

        mov r9.w, l1.y
        uav_store_id(0) r9, r1

        mov r9.w, l1.z
        uav_store_id(0) r9, r1

        mov r9.w, l1.w
        uav_store_id(0) r9, r1
        break

        default
        break
        endswitch
    break

    case 2

        mov r0.w, l0.x
        uav_load_id(1) r1, r0
        mov r0.w, l0.y
        uav_load_id(1) r2, r0

        switch cb0[1].w
        case 1

        mov r9.w, l0.x
        uav_store_id(0) r9, r1
        break

        case 4

        mov r9.w, l0.x
        uav_store_id(0) r9, r1

        mov r9.w, l0.y
        uav_store_id(0) r9, r1

        mov r9.w, l0.z
        uav_store_id(0) r9, r2

        mov r9.w, l0.w
        uav_store_id(0) r9, r2
        break

        case 8

        mov r9.w, l0.x
        uav_store_id(0) r9, r1

        mov r9.w, l0.y
        uav_store_id(0) r9, r1

        mov r9.w, l0.z
        uav_store_id(0) r9, r1

        mov r9.w, l0.w
        uav_store_id(0) r9, r1

        mov r9.w, l1.x
        uav_store_id(0) r9, r2

        mov r9.w, l1.y
        uav_store_id(0) r9, r2

        mov r9.w, l1.z
        uav_store_id(0) r9, r2

        mov r9.w, l1.w
        uav_store_id(0) r9, r2
        break

        default
        break
        endswitch
    break

    case 4
        mov r0.w, l0.x
        uav_load_id(1) r1, r0
        mov r0.w, l0.y
        uav_load_id(1) r2, r0
        mov r0.w, l0.z
        uav_load_id(1) r3, r0
        mov r0.w, l0.w
        uav_load_id(1) r4, r0

        switch cb0[1].w
        case 1

        mov r9.w, l0.x
        uav_store_id(0) r9, r1
        break

        case 2

        min r2, r1, r2
        min r4, r3, r4
        min r4, r2, r4

        mov r9.w, l0.x
        uav_store_id(0) r9, r1

        ; use min or max or sample 1???
        mov r9.w, l0.y
        uav_store_id(0) r9, r4
        break

        case 8

        mov r9.w, l0.x
        uav_store_id(0) r9, r1

        mov r9.w, l0.y
        uav_store_id(0) r9, r1

        mov r9.w, l0.z
        uav_store_id(0) r9, r2

        mov r9.w, l0.w
        uav_store_id(0) r9, r2

        mov r9.w, l1.x
        uav_store_id(0) r9, r3

        mov r9.w, l1.y
        uav_store_id(0) r9, r3

        mov r9.w, l1.z
        uav_store_id(0) r9, r4

        mov r9.w, l1.w
        uav_store_id(0) r9, r4
        break

        default
        break
        endswitch
    break

    case 8
        mov r0.w, l0.x
        uav_load_id(1) r1, r0
        mov r0.w, l0.y
        uav_load_id(1) r2, r0
        mov r0.w, l0.z
        uav_load_id(1) r3, r0
        mov r0.w, l0.w
        uav_load_id(1) r4, r0
        mov r0.w, l1.x
        uav_load_id(1) r5, r0
        mov r0.w, l1.y
        uav_load_id(1) r6, r0
        mov r0.w, l1.z
        uav_load_id(1) r7, r0
        mov r0.w, l1.w
        uav_load_id(1) r8, r0

        switch cb0[1].w
        case 1

        mov r9.w, l0.x
        uav_store_id(0) r9, r1
        break

        case 2

        min r2, r1, r2
        min r4, r3, r4
        min r6, r5, r6
        min r8, r7, r8

        min r4, r2, r4
        min r8, r6, r8

        min r8, r4, r8

        mov r9.w, l0.x
        uav_store_id(0) r9, r1

        mov r9.w, l0.y
        uav_store_id(0) r9, r8
        break

        case 4

        min r2, r2, r3
        min r4, r4, r5
        min r6, r6, r7
        min r8, r6, r8

        mov r9.w, l0.x
        uav_store_id(0) r9, r1

        mov r9.w, l0.y
        uav_store_id(0) r9, r2

        mov r9.w, l0.z
        uav_store_id(0) r9, r4

        mov r9.w, l0.w
        uav_store_id(0) r9, r8
        break

        default
        break
        endswitch
    break

    default
    break
    endswitch
endif
end

[ResourceMappingNodes]
rsrcInfo:
  - type:          Uav
    sizeInDwords:  8
    id:            0
  - type:          Uav
    sizeInDwords:  8
    id:            1
  - type:          InlineConst
    sizeInDwords:  4
    id:            0
    slot:          0
  - type:          InlineConst
    sizeInDwords:  4
    id:            0
    slot:          1

[ResourceMappingRootNodes]
  - type:          DescriptorTableVaPtr
    visibility:    cs
    sizeInDwords:  1
    pNext:         rsrcInfo
