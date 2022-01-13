//=============================================================================
/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
/// \author AMD Developer Tools Team
/// \file marker_payload.h
/// \brief Command buffer execution tracking marker payload declarations
//=============================================================================

#ifndef RGD_DRIVER_IFC_MARKER_PAYLOAD_H_
#define RGD_DRIVER_IFC_MARKER_PAYLOAD_H_

#include <stdint.h>

#define RGD_EXECUTION_BEGIN_MARKER_GUARD 380900298
#define RGD_EXECUTION_MARKER_GUARD 2697311323
#define RGD_EXECUTION_END_MARKER_GUARD 3180207583

/// This data is returned by the Vulkan/DX12 API driver to the caller
/// when a marker is inserted throught the API extension.
/// The DX12/Vulkan driver provides the client handle to PAL.
typedef struct RgdExecutionMarkerId
{
    uint64_t client_handle;  ///< API/driver command buffer handle
    uint32_t counter;  ///< End counter value
} RgdExecutionMarkerId;

typedef struct RgdExecutionBeginMarker
{
    uint32_t guard;  ///< Must be RGD_EXECUTION_BEGIN_MARKER_GUARD
    uint64_t marker_buffer;  ///< GPU buffer counter values are written to
    uint64_t client_handle;  ///< API/driver command buffer handle
    uint32_t counter;  ///< Counter value
} RgdExecutionBeginMarker;

/// Payload for NOP command buffer packet to track command buffer execution
typedef struct RgdExecutionMarker
{
    uint32_t guard;  ///< Must be RGD_EXECUTION_MARKER_GUARD
    uint32_t counter;  ///< Counter value
} RgdExecutionMarker;

/// Payload for NOP command buffer packet to track command buffer execution
typedef struct RgdExecutionEndMarker
{
    uint32_t guard;  ///< Must be RGD_EXECUTION_END_MARKER_GUARD
    uint32_t counter;  ///< End counter value
} RgdExecutionEndMarker;

#endif
