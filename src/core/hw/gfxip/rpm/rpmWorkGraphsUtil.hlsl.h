/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

// Note: This header gets compiled into the driver as a C++ header as well as in RPM shaders for work-graphs as an
// HLSL header.  The preprocessor symbol __cplusplus is defined whenever this is being compiled as a C++ header.
// When compiling with DirectXShaderCompiler, the `-enable-16bit-types` argument must be used for min16uint.
#ifndef __RPM_WORK_GRAPHS_UTIL_HLSL_H__
#define __RPM_WORK_GRAPHS_UTIL_HLSL_H__

#include "scheduler-v1/gfx10_3_work_graphs.hlsl.h"

#if __cplusplus
namespace Pal
{
namespace RpmUtil
{
#else
// This define avoids including unnecessary LDS fields in the RPM shaders.
#define USE_LDS_INTRINSICS
#include "scheduler-v1/util.hlsl.h"
#endif

struct CtrlRingInfo
{
    uint32_t  entry_count;
};

#define CTRL_RING_INFO_SIZE                 4
#define CTRL_RING_INFO_ENTRY_COUNT_OFFSET   0

#if __cplusplus
static_assert(CTRL_RING_INFO_SIZE               ==   sizeof(CtrlRingInfo),              "");
static_assert(CTRL_RING_INFO_ENTRY_COUNT_OFFSET == offsetof(CtrlRingInfo, entry_count), "");
#endif

struct ArrayInfo
{
    uint32_t  payload_entry_count;
};

#define ARRAY_INFO_SIZE                         4
#define ARRAY_INFO_PAYLOAD_ENTRY_COUNT_OFFSET   0

#if __cplusplus
static_assert(ARRAY_INFO_SIZE                       ==   sizeof(ArrayInfo),                      "");
static_assert(ARRAY_INFO_PAYLOAD_ENTRY_COUNT_OFFSET == offsetof(ArrayInfo, payload_entry_count), "");
#endif

struct NodeInfo
{
    gpuaddr_t  ring_address;
    uint32_t   payload_entry_count;
    uint32_t   payload_entry_stride;
#if __cplusplus
    dispatch_grid_info_t dispatch_grid_info;
#else
    uint32_t   dispatch_grid_info;
#endif
    uint32_t   _padding0;
};

#define NODE_INFO_SIZE                         24
#define NODE_INFO_RING_ADDRESS_OFFSET           0
#define NODE_INFO_PAYLOAD_ENTRY_COUNT_OFFSET    8
#define NODE_INFO_PAYLOAD_ENTRY_STRIDE_OFFSET  12
#define NODE_INFO_DISPATCH_GRID_INFO_OFFSET    16
#define NODE_INFO_PADDING0_OFFSET              20

#if __cplusplus
static_assert(NODE_INFO_SIZE                        ==   sizeof(NodeInfo),                       "");
static_assert(NODE_INFO_RING_ADDRESS_OFFSET         == offsetof(NodeInfo, ring_address),         "");
static_assert(NODE_INFO_PAYLOAD_ENTRY_COUNT_OFFSET  == offsetof(NodeInfo, payload_entry_count),  "");
static_assert(NODE_INFO_PAYLOAD_ENTRY_STRIDE_OFFSET == offsetof(NodeInfo, payload_entry_stride), "");
static_assert(NODE_INFO_DISPATCH_GRID_INFO_OFFSET   == offsetof(NodeInfo, dispatch_grid_info),   "");
static_assert(NODE_INFO_PADDING0_OFFSET             == offsetof(NodeInfo, _padding0),            "");
#endif

#if __cplusplus
using graph_data_p     = gpuaddr_t;
using ctrl_ring_info_p = gpuaddr_t;
using array_info_p     = gpuaddr_t;
using node_info_p      = gpuaddr_t;
#endif

struct InitGraphDataArgs
{
    /* out */ graph_data_p  graph;

    gpuaddr_t         ctrl_ring_base_addr; // Base GPU VA where all control rings start.
    ctrl_ring_info_p  ctrl_ring_infos;
    uint32_t          ctrl_ring_count;

    uint32_t  semaphore_count;
    uint32_t  queue_count;
    uint32_t  queue_rings_offset;
    uint32_t  queue_infos_offset;

#if __cplusplus
    graph_data_settings_t  graph_settings;
    log_settings_t         log_settings;
#else
    uint32_t  graph_settings0;
    uint32_t  log_settings0;
    uint32_t  log_settings1;
#endif
};

struct InitGraphArraysArgs
{
    /* in */ graph_data_p  graph;

    array_info_p  array_infos;
    uint32_t      array_start;
    uint32_t      array_count;
};

struct InitGraphNodesArgs
{
    /* in */ graph_data_p  graph;

    uint32_t  queue_rings_offset;
    uint32_t  queue_infos_offset;

    node_info_p  node_infos;
    uint32_t     node_start;
    uint32_t     node_count;
};

#if __cplusplus
} // RpmUtil
} // Pal
#endif

#endif
