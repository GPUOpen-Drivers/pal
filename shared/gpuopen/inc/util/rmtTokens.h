/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
***********************************************************************************************************************
* @file  rmtTokens.h
* @brief RMT token structure definitions
***********************************************************************************************************************
*/

#pragma once

#include <util/rmtCommon.h>
#include <util/rmtResourceDescriptions.h>

#define RMT_1KB 1024
#define RMT_4KB (4 * RMT_1KB)

namespace DevDriver
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT Types and Helper Functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Enumeration encoding values used for the RMT_TOKEN_TYPE fields
enum RMT_TOKEN_TYPE
{
    RMT_TOKEN_TIMESTAMP          = 0,
    RMT_TOKEN_RESERVED_0         = 1,
    RMT_TOKEN_RESERVED_1         = 2,
    RMT_TOKEN_PAGE_TABLE_UPDATE  = 3,
    RMT_TOKEN_USERDATA           = 4,
    RMT_TOKEN_MISC               = 5,
    RMT_TOKEN_RESOURCE_REFERENCE = 6,
    RMT_TOKEN_RESOURCE_BIND      = 7,
    RMT_TOKEN_PROCESS_EVENT      = 8,
    RMT_TOKEN_PAGE_REFERENCE     = 9,
    RMT_TOKEN_CPU_MAP            = 10,
    RMT_TOKEN_FREE_VIRTUAL       = 11,
    RMT_TOKEN_VIRTUAL_ALLOCATE   = 12,
    RMT_TOKEN_RESOURCE_CREATE    = 13,
    RMT_TOKEN_TIME_DELTA         = 14,
    RMT_TOKEN_RESOURCE_DESTROY   = 15,
};

// Enumeration of process event types
enum RMT_PROCESS_EVENT_TYPE
{
    RMT_PROCESS_EVENT_TYPE_START = 0,
    RMT_PROCESS_EVENT_TYPE_END   = 1,
};

// For string userdata event types (Name and Snapshot) we define a reasonable max size
// This size is chosen to have plenty of room for data that we might need.
// Including, but not limited to:
//      pointers for a callstack
//      one or more file paths
//      long debug resource names
#define RMT_MAX_USERDATA_STRING_SIZE 1024u

// For debug name event types, we need to reserve an extra five bytes at the end of the buffer in order to
// encode the resource id into the data. 1 byte for a NULL to maintain string compatibility, and another 4
// bytes for the resource id value.
#define RMT_ENCODED_RESOURCE_ID_SIZE (static_cast<uint32>(sizeof(uint32) + 1))

// Enumeration of Userdata event types
enum RMT_USERDATA_EVENT_TYPE
{
    RMT_USERDATA_EVENT_TYPE_NAME       = 0,
    RMT_USERDATA_EVENT_TYPE_SNAPSHOT   = 1,
    RMT_USERDATA_EVENT_TYPE_BINARY     = 2,
    RMT_USERDATA_EVENT_TYPE_CALL_STACK = 3,
};

// Enumeration of the MISC event types
enum RMT_MISC_EVENT_TYPE
{
    RMT_MISC_EVENT_TYPE_SUBMIT_GFX                 = 0,
    RMT_MISC_EVENT_TYPE_SUBMIT_COMPUTE             = 1,
    RMT_MISC_EVENT_TYPE_SUBMIT_COPY                = 2,
    RMT_MISC_EVENT_TYPE_PRESENT                    = 3,
    RMT_MISC_EVENT_TYPE_INVALIDATE_RANGES          = 4,
    RMT_MISC_EVENT_TYPE_FLUSH_MAPPED_MEMORY_RANGED = 5,
    RMT_MISC_EVENT_TYPE_TRIM_MEMORY                = 6,
    RMT_MISC_EVENT_TYPE_PROFILE_START              = 7,
    RMT_MISC_EVENT_TYPE_PROFILE_END                = 8,
};

// Enumeration of RMT owner values
enum RMT_OWNER
{
    RMT_OWNER_APP           = 0,
    RMT_OWNER_PAL           = 1,
    RMT_OWNER_CLIENT_DRIVER = 2,
    RMT_OWNER_KMD           = 3,
};

// Enumeration of RMT commit types
enum RMT_COMMIT_TYPE
{
    RMT_COMMIT_TYPE_COMMITTED = 0,
    RMT_COMMIT_TYPE_PLACED    = 1,
    RMT_COMMIT_TYPE_VIRTUAL   = 2,
};

// Enumeration of RMT resource types
enum RMT_RESOURCE_TYPE
{
    RMT_RESOURCE_TYPE_IMAGE                  = 0,
    RMT_RESOURCE_TYPE_BUFFER                 = 1,
    RMT_RESOURCE_TYPE_GPU_EVENT              = 2,
    RMT_RESOURCE_TYPE_BORDER_COLOR_PALETTE   = 3,
    RMT_RESOURCE_TYPE_INDIRECT_CMD_GENERATOR = 4,
    RMT_RESOURCE_TYPE_MOTION_ESTIMATOR       = 5,
    RMT_RESOURCE_TYPE_PERF_EXPERIMENT        = 6,
    RMT_RESOURCE_TYPE_QUERY_HEAP             = 7,
    RMT_RESOURCE_TYPE_VIDEO_DECODER          = 8,
    RMT_RESOURCE_TYPE_VIDEO_ENCODER          = 9,
    RMT_RESOURCE_TYPE_TIMESTAMP              = 10,
    RMT_RESOURCE_TYPE_HEAP                   = 11,
    RMT_RESOURCE_TYPE_PIPELINE               = 12,
    RMT_RESOURCE_TYPE_DESCRIPTOR_HEAP        = 13,
    RMT_RESOURCE_TYPE_DESCRIPTOR_POOL        = 14,
    RMT_RESOURCE_TYPE_CMD_ALLOCATOR          = 15,
    RMT_RESOURCE_TYPE_MISC_INTERNAL          = 16
};

// This is the number of clocks timestamps will be expressed as
#define RMT_TIME_UNIT 32

// The threshold for a timestamp delta to trigger a RMT_MSG_TIME_DELTA token
// Each token has 4 bits of DELTA, which can encode up to 15 DELTA_UNITS
#define RMT_TIME_DELTA_THRESHOLD (15 * RMT_TIME_UNIT)

// The threshold that triggers a RMT_MSG_TIMESTAMP_TOKEN
#define RMT_TIMESTAMP_THRESHOLD ((1ull << 56) - 1)

struct RMT_TOKEN_HEADER
{
    union
    {
        struct
        {
            uint8 TOKEN_TYPE : 4; // [3:0]  RMT_TOKEN_TYPE enum
            uint8 DELTA      : 4; // [7:4]  Delta from the last TIMESTAMP/TIME_DELTA, in RMT_TIME_UNITs
        };

        uint8 byteVal;
    };

    RMT_TOKEN_HEADER(RMT_TOKEN_TYPE type, uint8 delta)
        : TOKEN_TYPE(static_cast<uint8>(type))
        , DELTA(delta)
    {
        DD_ASSERT(delta < (1ull << 4));
    }
};

enum RMT_PAGE_TABLE_UPDATE_TYPE
{
    RMT_PAGE_TABLE_UPDATE_TYPE_DISCARD          = 0,
    RMT_PAGE_TABLE_UPDATE_TYPE_UPDATE           = 1,
    RMT_PAGE_TABLE_UPDATE_TYPE_TRANSFER         = 2,
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global Token Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_TIMESTAMP
static const size_t RMT_MSG_TIMESTAMP_TOKEN_BYTES_SIZE = 96 / 8; // 96-bits
struct RMT_MSG_TIMESTAMP : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_TIMESTAMP_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_TIMESTAMP(uint64 timestamp, uint64 frequency)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0]  RMT_TOKEN_TYPE enum
        SetBits(RMT_TOKEN_TIMESTAMP, 3, 0);

        // TIMESTAMP [63:4]  The timestamp of the token in RMT_TIME_UNITs
        SetBits(timestamp, 63, 4);

        // Frequency [95:64] The lower 32 bits of CPU frequency
        SetBits(frequency, 95, 64);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_TIME_DELTA
static const size_t RMT_MSG_TIME_DELTA_TOKEN_MAX_BYTES_SIZE = 56 / 8; // Max of 56-bits
struct RMT_MSG_TIME_DELTA : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_TIME_DELTA_TOKEN_MAX_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_TIME_DELTA(uint64 delta, uint8 numDeltaBytes)
    {
        // The actual size is the 1 + the delta bytes
        sizeInBytes = numDeltaBytes + 1;
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0]  RMT_TOKEN_TYPE enum
        SetBits(RMT_TOKEN_TIME_DELTA, 3, 0);

        // DELTA_BYTES [6:4]  The number of bytes of delta that follow in this token.  Max of 6.
        SetBits(numDeltaBytes, 6, 4);

        // RESERVED [7]  Reserved for future expansion. Should be set to 0.
        SetBits(0, 7, 7);

        // DELTA [NumDeltaBits:8]  The delta from the last token in RMT_TIME_UNITs
        uint32 endBit = (numDeltaBytes * 8) + 7;
        SetBits(delta, endBit, 8);
    }
};

// A special delta value used to indicate that this TIME_DELTA token should be combined with following ones.
#define RMT_TIME_DELTA_CHAIN_VALUE ((1ull << 12) - 1)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_USERDATA
static const size_t RMT_MSG_USERDATA_TOKEN_BYTES_SIZE = 32 / 8; // 32-bits
struct RMT_MSG_USERDATA : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_USERDATA_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_USERDATA(uint8 delta, RMT_USERDATA_EVENT_TYPE type, uint32 payloadSize)
    {
        sizeInBytes = sizeof(bytes);
        pByteData   = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_USERDATA, delta);
        SetBits(header.byteVal, 7, 0);

        // TYPE[11:8] The type of the user data being emitted encoded as RMT_USERDATAYPE.
        SetBits(type, 11, 8);

        // PAYLOAD_SIZE[31:12] The size of the payload that immediately follows this token, expressed in bytes.
        SetBits(payloadSize, 31, 12);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_USERDATA with an embedded non-NULL-terminated string value
struct RMT_MSG_USERDATA_EMBEDDED_STRING : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_USERDATA_TOKEN_BYTES_SIZE + RMT_MAX_USERDATA_STRING_SIZE];

    // Initializes the token fields
    RMT_MSG_USERDATA_EMBEDDED_STRING(uint8 delta, RMT_USERDATA_EVENT_TYPE type, const char* pString)
    {
        // Store the string
        uint32 payloadSize = static_cast<uint32>(strlen(pString));

        // Truncate long payloads so that they fit
        DD_WARN(payloadSize <= RMT_MAX_USERDATA_STRING_SIZE);
        payloadSize = Platform::Min(payloadSize, RMT_MAX_USERDATA_STRING_SIZE);

        sizeInBytes = RMT_MSG_USERDATA_TOKEN_BYTES_SIZE + payloadSize;
        pByteData   = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_USERDATA, delta);
        SetBits(header.byteVal, 7, 0);

        // TYPE[11:8] The type of the user data being emitted encoded as RMT_USERDATAYPE.
        SetBits(type, 11, 8);

        // PAYLOAD_SIZE[31:12] The size of the payload that immediately follows this token, expressed in bytes.
        SetBits(payloadSize, 31, 12);

        memcpy(&bytes[RMT_MSG_USERDATA_TOKEN_BYTES_SIZE], pString, payloadSize);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_USERDATA variant for debug names
struct RMT_MSG_USERDATA_DEBUG_NAME : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_USERDATA_TOKEN_BYTES_SIZE + RMT_MAX_USERDATA_STRING_SIZE + RMT_ENCODED_RESOURCE_ID_SIZE];

    // Initializes the token fields
    RMT_MSG_USERDATA_DEBUG_NAME(uint8 delta, const char* pDebugName, uint32 resourceId)
    {
        // Make sure the true encoded size always matches our define
        static_assert((sizeof(resourceId) + 1) == RMT_ENCODED_RESOURCE_ID_SIZE,
                      "Encoded resource id size is incorrect!");

        // Calcualte the string storage size
        uint32 stringSize = static_cast<uint32>(strlen(pDebugName));

        // Truncate long strings so that they fit
        DD_WARN(stringSize <= RMT_MAX_USERDATA_STRING_SIZE);
        stringSize = Platform::Min(stringSize, RMT_MAX_USERDATA_STRING_SIZE);

        const uint32 payloadSize = stringSize + RMT_ENCODED_RESOURCE_ID_SIZE;

        sizeInBytes = RMT_MSG_USERDATA_TOKEN_BYTES_SIZE + payloadSize;
        pByteData   = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_USERDATA, delta);
        SetBits(header.byteVal, 7, 0);

        // TYPE[11:8] The type of the user data being emitted encoded as RMT_USERDATAYPE.
        SetBits(RMT_USERDATA_EVENT_TYPE_NAME, 11, 8);

        // PAYLOAD_SIZE[31:12] The size of the payload that immediately follows this token, expressed in bytes.
        SetBits(payloadSize, 31, 12);

        // Copy the debug name into the payload first
        memcpy(&bytes[RMT_MSG_USERDATA_TOKEN_BYTES_SIZE], pDebugName, stringSize);

        // Append a NULL byte to the end of the string to maintain compatibility
        bytes[RMT_MSG_USERDATA_TOKEN_BYTES_SIZE + stringSize] = '\0';

        // Insert the resource id into the payload after the NULL
        memcpy(&bytes[RMT_MSG_USERDATA_TOKEN_BYTES_SIZE + stringSize + 1], &resourceId, sizeof(resourceId));
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_MISC
static const size_t RMT_MSG_MISC_TOKEN_BYTES_SIZE = 16 / 8; // 16-bits
struct RMT_MSG_MISC : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_MISC_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_MISC(uint8 delta, RMT_MISC_EVENT_TYPE type)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_MISC, delta);
        SetBits(header.byteVal, 7, 0);

        // MISC_EVENT[11:8] The event that took place encoded as a RMT_MISC_EVENT_TYPE.
        SetBits(type, 11, 8);

        // RESERVED[15:12] Reserved for future expansion. Should be set to 0.
        SetBits(0, 15, 12);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// KMD Token Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_PAGE_TABLE_UPDATE
static const size_t RMT_MSG_PAGE_TABLE_UPDATE_TOKEN_BYTES_SIZE = 144 / 8;  // 144-bits
struct RMT_MSG_PAGE_TABLE_UPDATE : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_PAGE_TABLE_UPDATE_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_PAGE_TABLE_UPDATE(
        uint8                      delta,
        uint64                     virtualAddress,
        uint64                     physicalAddress,
        uint32                     size,
        RMT_PAGE_SIZE              pageSize,
        bool                       isUnmap,
        uint32                     processId,
        RMT_PAGE_TABLE_UPDATE_TYPE type,
        bool                       isHbccMode)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_PAGE_TABLE_UPDATE, delta);
        SetBits(header.byteVal, 7, 0);

        // VIRTUAL_ADDRESS [43:8] Bits [47:12] of the 48bit virtual address that is being mapped.
        SetBits((virtualAddress >> 12), 43, 8);

        // PHYSICAL_ADDRESS [79:44] Bits [47:12] of the 48bit physical address of the allocation that VIRTUAL_ADDRESS
        //                            is being mapped to. In the cases of system memory, this field should be programmed
        //                            to 0.
        SetBits((physicalAddress >> 12), 79, 44);

        // SIZE [99:80] The size of the allocation specified in pages.
        SetBits(size, 99, 80);

        // PAGE_SIZE [102:100] The size of the page expressed as a RMT_PAGE_SIZE enum
        SetBits(pageSize, 102, 100);

        // UNMAP [103] If this bit is set, this page table update is a local memory unmapping operation.
        SetBits((isUnmap ? 1 : 0), 103, 103);

        // PROCESS_ID [135:104] The ID of the process. For Windows this is a 32bit value. For Linux this could be a
        //                        22bit value for 32bit machines. Not sure currently for 64bit Linux distributions.
        SetBits(processId, 135, 104);

        // TYPE [137:136] The type of free, see RMT_PAGE_TABLE_UPDATE_TYPE.
        SetBits(type, 137, 136);

        // PAGING_CONTROL [138] If bit set, this indicates that the KMD is is controlling page table update
        //                            decisions. If the bit is not set, then the operating system is dictating them.
        SetBits((isHbccMode ? 1 : 0), 138, 138);

        // RESERVED [143:139] Reserved for future expansion. Should be set to 0.
        SetBits(0, 143, 139);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_PROCESS_EVENT
static const size_t RMT_MSG_PROCESS_EVENT_TOKEN_BYTES_SIZE = 48 / 8;  // 48-bits
struct RMT_MSG_PROCESS_EVENT : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_PROCESS_EVENT_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_PROCESS_EVENT(
        uint8                delta,
        uint32               processId,
        RMT_PROCESS_EVENT_TYPE type)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_PROCESS_EVENT, delta);
        SetBits(header.byteVal, 7, 0);

        // PROCESS_ID [39:8] The ID of the process. For Windows this is a 32bit value. For Linux this could be a
        //                        22bit value for 32bit machines. Not sure currently for 64bit Linux distributions.
        SetBits(processId, 39, 8);

        // EVENTYPE[47:40] The type of process event that occurred for this process encoded as a
        //                     RMT_PROCESS_EVENT_TYPE enum.
        SetBits(type, 47, 40);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_PAGE_REFERENCE
static const size_t RMT_MSG_PAGE_REFERENCE_TOKEN_BYTES_SIZE = 80 / 8;  // 80-bits
struct RMT_MSG_PAGE_REFERENCE : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_PAGE_REFERENCE_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_PAGE_REFERENCE(
        uint8         delta,
        RMT_PAGE_SIZE pageSize,
        bool          isCompressed,
        uint64        pageRefData)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_PAGE_REFERENCE, delta);
        SetBits(header.byteVal, 7, 0);

        // PAGE_SIZE [10:8] The size of the pages (see Table 3). This does not have to be the same as the physical
        //                    memory page size. An implementation may opt for a larger size to increase the memory
        //                    range it can fit into this encoding to reduce the memory footprint.
        SetBits(pageSize, 10, 8);

        // COMPRESSED [11] Bit that indicates if subsequent 64bit payload is compressed or not. For details on the
        //                   compression scheme please refer to the specification for the PAGE_STATE_COMPRESSED specification.
        SetBits((isCompressed ? 1 : 0), 11, 11);

        // RESERVED [15:12] Reserved for future expansion. Should be set to 0 by any implementation.
        SetBits(0, 15, 12);

        if (isCompressed)
        {
            // PAGE_STATE_COMPRESSED [75:16] See the RMT specification for details on encoding for compressed page state.
            SetBits(pageRefData, 75, 16);

            // RESERVED_COMPRESSED [79:76] These bits are unused in COMPRESSED mode. Should be set to 0.
            SetBits(0, 79, 76);
        }
        else
        {
            // PAGE_STATE_UNCOMPRESSED [79:16] One bit per page from the last calculated physical base address. If this
            //                                   is the first token, then the base address will be 0.
            SetBits(pageRefData, 79, 16);
        }
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UMD Token Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_RESOURCE_REFERENCE
static const size_t RMT_MSG_RESOURCE_REFERENCE_TOKEN_BYTES_SIZE = 64 / 8; // 64-bits
struct RMT_MSG_RESOURCE_REFERENCE : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_RESOURCE_REFERENCE_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_RESOURCE_REFERENCE(uint8 delta, bool isRemove, uint64 virtualAddress, uint8 queueId)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_RESOURCE_REFERENCE, delta);
        SetBits(header.byteVal, 7, 0);

        // ADD_OR_REMOVE [8] A bit denoting if this was an add reference or remove reference
        //                     0 - Add reference (MakeResident in Windows/DirectX12 lexicon).
        //                     1 - Remove reference (Evict in Windows/DirectX12 lexicon).
        SetBits((isRemove ? 1 : 0), 8, 8);

        // VIRTUAL_ADDRESS [56:9] The 48bit virtual address of the memory that is being made resident/evicted.
        SetBits(virtualAddress, 56, 9);

        // QUEUE_ID [63:57] This is a unique identifier for the OS queue (which subsequently maps to the HW queue)
        //                    that the memory is being referenced by.
        SetBits(queueId, 63, 57);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_RESOURCE_BIND
static const size_t RMT_MSG_RESOURCE_BIND_TOKEN_BYTES_SIZE = 136 / 8; // 136-bits
struct RMT_MSG_RESOURCE_BIND : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_RESOURCE_BIND_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_RESOURCE_BIND(uint8 delta, uint64 virtualAddress, uint64 size, uint32 resourceId, bool isSystemMemory)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_RESOURCE_BIND, delta);
        SetBits(header.byteVal, 7, 0);

        // VIRTUAL_ADDRESS [55:8] The 48bit virtual address of the memory that is bound to the resource.
        SetBits(virtualAddress, 55, 8);

        // SIZE [99:56] The size, in bytes, of the binding of memory to the resource. Maximum size is 48bits to match
        //                 address sizes.
        SetBits(size, 99, 56);

        // FLAGS [103:100]
        //   Bit 0 When set indicates that CPU system memory is being bound to the resource
        //   Bits 1-3 Reserved for future expansion. Should be set to 0.
        SetBits(isSystemMemory, 100, 100);
        SetBits(0, 103, 101);

        // RESOURCE_IDENTIFIER [135:104] A unique identifier for the resource. This allows this token to be correlated
        //                                 with RMT_TOKEN_TYPE_RESOURCE_CREATE tokens later in the RMT stream.
        SetBits(resourceId, 135, 104);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_CPU_MAP
static const size_t RMT_MSG_CPU_MAP_TOKEN_BYTES_SIZE = 64 / 8; // 64-bits
struct RMT_MSG_CPU_MAP : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_CPU_MAP_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_CPU_MAP(uint8 delta, uint64 virtualAddress, bool isUnmap)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_CPU_MAP, delta);
        SetBits(header.byteVal, 7, 0);

        // VIRTUAL_ADDRESS [55:8] The 48bit virtual address of the memory that is being mapped/unmapped.
        SetBits(virtualAddress, 55, 8);

        // IS_UNMAP [56] 0 - Indicates that this is an MAP operation.
        //               1 - Indicates that this is an UNMAP operation.
        SetBits((isUnmap ? 1 : 0), 56, 56);

        // RESERVED [63:57] Reserved for future expansion. Should be set to 0.
        SetBits(0, 63, 57);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_FREE_VIRTUAL
static const size_t RMT_MSG_FREE_VIRTUAL_TOKEN_BYTES_SIZE = 56 / 8; // 56-bits
struct RMT_MSG_FREE_VIRTUAL : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_FREE_VIRTUAL_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_FREE_VIRTUAL(uint8 delta, uint64 virtualAddress)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_FREE_VIRTUAL, delta);
        SetBits(header.byteVal, 7, 0);

        // VIRTUAL_ADDRESS [55:8] The 48bit virtual address of the allocation that was freed.
        SetBits(virtualAddress, 55, 8);

    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_VIRTUAL_ALLOCATE
static const size_t RMT_MSG_VIRTUAL_ALLOCATE_TOKEN_BYTES_SIZE = 96 / 8; // 96-bits
struct RMT_MSG_VIRTUAL_ALLOCATE : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_VIRTUAL_ALLOCATE_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_VIRTUAL_ALLOCATE(
        uint8         delta,
        uint64        size,
        RMT_OWNER     owner,
        uint64        virtualAddress,
        RMT_HEAP_TYPE heapImportance0,
        RMT_HEAP_TYPE heapImportance1,
        RMT_HEAP_TYPE heapImportance2,
        RMT_HEAP_TYPE heapImportance3,
        uint8         heapImportanceCount)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_VIRTUAL_ALLOCATE, delta);
        SetBits(header.byteVal, 7, 0);

        // SIZE [31:8] The size of the allocation specified in 4KiB chunks - 1.
        const uint32 rmtSize = static_cast<uint32>(size / RMT_4KB) - 1;
        SetBits(rmtSize, 31, 8);

        // OWNER [33:32] Which part of the software stack is making the request for allocation, encoded as RMT_OWNER
        SetBits(owner, 33, 32);

        // VIRTUAL_ADDRESS [81:34] The 48bit virtual address of the allocation.
        SetBits(virtualAddress, 81, 34);

        // HEAP_IMPORTANCE_0 [83:82] The highest priority heap for this allocation, encoded as RMT_HEAP_TYPE.
        SetBits(heapImportance0, 83, 82);

        // HEAP_IMPORTANCE_1 [85:84] The second priority heap for this allocation, encoded as RMT_HEAP_TYPE.
        SetBits(heapImportance1, 85, 84);

        // HEAP_IMPORTANCE_2 [87:86] The third priority heap for this allocation, encoded as RMT_HEAP_TYPE.
        SetBits(heapImportance2, 87, 86);

        // HEAP_IMPORTANCE_3 [89:88] The lowest priority heap for this allocation, encoded as RMT_HEAP_TYPE.
        SetBits(heapImportance3, 89, 88);

        // HEAP_IMPORTANCE_COUNT [92:90] The number of heap importance fields that are in use.
        SetBits(heapImportanceCount, 92, 90);

        // RESERVED [95:93] Reserved for future expansion. Should be set to 0.
        SetBits(0, 95, 93);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_RESOURCE_CREATE
static const size_t RMT_MSG_RESOURCE_CREATE_TOKEN_BYTES_SIZE = 56 / 8; // 56-bits
struct RMT_MSG_RESOURCE_CREATE : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_RESOURCE_CREATE_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_RESOURCE_CREATE(
        uint8             delta,
        uint32            resourceId,
        RMT_OWNER         owner,
        uint8             ownerCategory,
        RMT_COMMIT_TYPE   commitType,
        RMT_RESOURCE_TYPE resourceType)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_RESOURCE_CREATE, delta);
        SetBits(header.byteVal, 7, 0);

        // RESOURCE_IDENTIFIER [39:8] A unique identifier for the resource. This allows this token to be correlated with
        //                              RMT_TOKEN_TYPE_RESOURCE_BIND tokens later in the RMT stream.
        SetBits(resourceId, 39, 8);

        // OWNER [41:40] Which part of the software stack owns the resource (see Table 7.)
        SetBits(owner, 41, 40);

        // OWNER_CATEGORY [45:42] The category of the owner for this resource. This allows for more granular encoding of
        //                          which part of the stack owns the resource.
        SetBits(ownerCategory, 45, 42);

        // COMMIT_TYPE [47:46] How the resource was committed (encoded as RMT_COMMIT_TYPE.
        SetBits(commitType, 47, 46);

        // RESOURCE_TYPE [53:48] The type of resource that is being described. Based on this the payload following the
        //                         token will change format to encapsulate different set of parameters for the resource.
        //                         encoded as RMT_RESOURCE_TYPE.
        SetBits(resourceType, 53, 48);

        // RESERVED [55:54] Reserved for future expansion. Should be set to 0.
        SetBits(0, 55, 54);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RMT_MSG_RESOURCE_DESTROY
static const size_t RMT_MSG_RESOURCE_DESTROY_TOKEN_BYTES_SIZE = 40 / 8; // 40-bits
struct RMT_MSG_RESOURCE_DESTROY : RMT_TOKEN_DATA
{
    uint8 bytes[RMT_MSG_RESOURCE_DESTROY_TOKEN_BYTES_SIZE];

    // Initializes the token fields
    RMT_MSG_RESOURCE_DESTROY(uint8 delta, uint32 resourceId)
    {
        sizeInBytes = sizeof(bytes);
        pByteData = &bytes[0];

        // RMT_TOKEN_TYPE [3:0] Token type (see Table 2). Encoded to RMT_TOKEN_TYPE_ALLOCATE.
        // DELTA      [7:4] The delta from the last token. In increments of 32-time units.
        RMT_TOKEN_HEADER header(RMT_TOKEN_RESOURCE_DESTROY, delta);
        SetBits(header.byteVal, 7, 0);

        // RESOURCE_IDENTIFIER [39:8] The unique identifier for the resource being destroyed.
        SetBits(resourceId, 39, 8);
    }
};

} // namespace DevDriver
