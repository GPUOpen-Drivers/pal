//=============================================================================
/// Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
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

#endif  // RGD_DRIVER_IFC_MARKER_PAYLOAD_H_
