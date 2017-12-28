/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef _SDMA10_PKT_H_
#define _SDMA10_PKT_H_

#ifdef __cplusplus
extern "C" {
#endif  //  __cplusplus

//
//  All definitions in this file are common to R6xx, R7xx and Evergreen family
//  except where otherwise noted.
//

/////////////////////////////////////////////////////////////////////////////////
// Values for command packet header 'type' field
/////////////////////////////////////////////////////////////////////////////////

enum DMA_COMMAND_TYPE
{
    DMA_COMMAND_WRITE                     = 0x02,
    DMA_COMMAND_COPY                      = 0x03,
    DMA_COMMAND_INDIRECT_BUFFER           = 0x04,
    DMA_COMMAND_SEMAPHORE                 = 0x05,
    DMA_COMMAND_FENCE                     = 0x06,
    DMA_COMMAND_TRAP                      = 0x07,
    DMA_COMMAND_SRBM_WRITE                = 0x09,
    DMA_COMMAND_CONDITIONAL_EXECUTION     = 0x0C,
    DMA_COMMAND_CONSTANT_FILL             = 0x0D,
    DMA_COMMAND_POLL_REG_MEM              = 0x0E,
    DMA_COMMAND_NOP                       = 0x0F,
};

/////////////////////////////////////////////////////////////////////////////////
// pre-Evergreen packet header definition
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
    unsigned int  count     : 16;   // transfer size in DWORDS
    unsigned int  reserved1 : 3;    // Reserved
    unsigned int  write     : 1;    // Write-1
    unsigned int  reserved2 : 2;    // Reserved
    unsigned int  semaphore : 1;    // Semaphore
    unsigned int  tiling    : 1;    // Tiling/Detiling enable
    unsigned int  reserved3 : 2;    // Reserved
    unsigned int  r8xxcmd   : 1;    // Evergreen command type
    unsigned int  reserved4 : 1;    // Reserved
    unsigned int  type      : 4;    // command code
#else
    unsigned int  type      : 4;    // command code
    unsigned int  reserved4 : 1;    // Reserved
    unsigned int  r8xxcmd   : 1;    // Evergreen command type
    unsigned int  reserved3 : 2;    // Reserved
    unsigned int  tiling    : 1;    // Tiling/Detiling enable
    unsigned int  semaphore : 1;    // Semaphore
    unsigned int  reserved2 : 2;    // Reserved
    unsigned int  write     : 1;    // Write-1
    unsigned int  reserved1 : 3;    // Reserved
    unsigned int  count     : 16;   // transfer size in DWORDS
#endif
    } bits;
    unsigned int u32All;
} DMA_CMD_HEADER;

/////////////////////////////////////////////////////////////////////////////////
// Evergreen-specific packet header definition
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int count         : 20;    // count in DWORDs
        unsigned int reserved1     : 1;     // reserved
        unsigned int write         : 1;     // Write-1
        unsigned int semaphore     : 1;     // semaphore
        unsigned int tiling        : 1;     // tiling
        unsigned int reserved2     : 2;     // Reserved
        unsigned int r8xxcmd       : 1;     // Evergreen command type
        unsigned int reserved3     : 1;     // Reserved
        unsigned int type          : 4;     // DMA_HEADER_TYPE
#else
        unsigned int type          : 4;     // DMA_HEADER_TYPE
        unsigned int reserved3     : 1;     // Reserved
        unsigned int r8xxcmd       : 1;     // Evergreen command type
        unsigned int reserved2     : 2;     // Reserved
        unsigned int tiling        : 1;     // tiling
        unsigned int semaphore     : 1;     // semaphore
        unsigned int write         : 1;     // Write-1
        unsigned int reserved1     : 1;     // reserved
        unsigned int count         : 20;    // count in DWORDs
#endif
    } bits;
    unsigned int u32All;
}  DMA_CMD_HEADER_EVERGREEN;

/////////////////////////////////////////////////////////////////////////////////
// Cayman-specific packet header definition
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int count         : 20;    // count in DWORDs
        unsigned int idcmd         : 3;     // vmid or copy command
        unsigned int tiling        : 1;     // tiling
        unsigned int reserved1     : 2;     // reserved
        unsigned int r8xxcmd       : 1;     // Evergreen command type
        unsigned int reserved2     : 1;     // reserved
        unsigned int type          : 4;     // DMA_HEADER_TYPE
#else
        unsigned int type          : 4;     // DMA_HEADER_TYPE
        unsigned int reserved2     : 1;     // reserved
        unsigned int r8xxcmd       : 1;     // Evergreen command type
        unsigned int reserved1     : 2;     // reserved
        unsigned int tiling        : 1;     // tiling
        unsigned int idcmd         : 3;     // vmid or copy command
        unsigned int count         : 20;    // count in DWORDs
#endif
    } bits;
    unsigned int u32All;
}  DMA_CMD_HEADER_CAYMAN;

/////////////////////////////////////////////////////////////////////////////////
// SI-specific packet header definition
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int count         : 20;    // count in DWORDs
        unsigned int vmid          : 4;     // vmid
        unsigned int reserved      : 4;     // reserved
        unsigned int type          : 4;     // DMA_HEADER_TYPE
#else
        unsigned int type          : 4;     // DMA_HEADER_TYPE
        unsigned int reserved      : 4;     // reserved
        unsigned int vmid          : 4;     // vmid
        unsigned int count         : 20;    // count in DWORDs
#endif
    } bits;
    unsigned int u32All;
}  DMA_CMD_HEADER_SI_IB;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int count         : 14;    // count in DWORDs
        unsigned int reserved      : 14;    // reserved
        unsigned int type          : 4;     // DMA_HEADER_TYPE
#else
        unsigned int type          : 4;     // DMA_HEADER_TYPE
        unsigned int reserved      : 14;    // reserved
        unsigned int count         : 14;    // count in DWORDs
#endif
    } bits;
    unsigned int u32All;
}  DMA_CMD_HEADER_SI_COND_EXE;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int count         : 20;    // count in DWORDs
        unsigned int reserved      : 6;     // reserved
        unsigned int srbm_poll     : 1;     // SRBM Poll Bit
        unsigned int srbm_write    : 1;     // Srbm Write
        unsigned int type          : 4;     // DMA_HEADER_TYPE
#else
        unsigned int type          : 4;     // DMA_HEADER_TYPE
        unsigned int srbm_write    : 1;     // Srbm Write
        unsigned int srbm_poll     : 1;     // SRBM Poll Bit
        unsigned int reserved      : 6;     // reserved
        unsigned int count         : 20;    // count in DWORDs
#endif
    } bits;
    unsigned int u32All;
}  DMA_CMD_HEADER_CAYMAN_SRBM;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int count         : 20;    // count in DWORDs
        unsigned int reserved1     : 1;     // reserved
        unsigned int write1algo    : 1;     // Selects Write1  algorithm or Increment
        unsigned int signal        : 1;     // Signal or wait
        unsigned int mailbox_check : 1;     // MailBox Check enable bit
        unsigned int reserved2     : 4;     // Reserved
        unsigned int type          : 4;     // DMA_HEADER_TYPE
#else
        unsigned int type          : 4;     // DMA_HEADER_TYPE
        unsigned int reserved2     : 4;     // reserved
        unsigned int mailbox_check : 1;     // MailBox Check enable bit
        unsigned int signal        : 1;     // Signal or wait
        unsigned int write1algo    : 1;     // Selects Write1  algorithm or Increment
        unsigned int reserved1     : 1;     // reserved
        unsigned int count         : 20;    // count in DWORDs
#endif
    } bits;
    unsigned int u32All;
}  DMA_CMD_HEADER_CAYMAN_SEMAPHORE;

/////////////////////////////////////////////////////////////////////////////////
// Common packet header definition, allowing usage of same packet body
// definitions on GPUs that require different header formats
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    DMA_CMD_HEADER                  header7xx;
    DMA_CMD_HEADER_EVERGREEN        headerEvergreen;
    DMA_CMD_HEADER_CAYMAN           headerCayman;
    DMA_CMD_HEADER_SI_IB            ibHeaderSI;
    DMA_CMD_HEADER_CAYMAN_SRBM      srbmHeaderCayman;
    DMA_CMD_HEADER_CAYMAN_SEMAPHORE semaphoreHeaderCayman;
} DMA_GPUSPECIFIC_PACKET_HEADER;

/////////////////////////////////////////////////////////////////////////////////
// Conditional Execution command
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int reserved        : 2;   // reserved
        unsigned int AddrLo          : 30;  // Dst Address bits [31-2]
#else
        unsigned int AddrLo          : 30;  // Dst Address bits [31-2]
        unsigned int reserved        : 2;   // reserved
#endif
    } bits;
    unsigned int u32All;
} CONDITIONAL_EXECUTION_ADDR_LOW;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int  AddrHi       : 8;    // bits 39:32
        unsigned int  reserved     : 24;   // Set to zero
#else
        unsigned int  reserved     : 24;   // Set to zero
        unsigned int  AddrHi       : 8;    // bits 39:32
#endif
    } bits;
    unsigned int u32All;
} CONDITIONAL_EXECUTION_ADDR_HIGH;

typedef struct _DMA_CMD_CONDITIONAL_EXECUTION
{
    DMA_CMD_HEADER_SI_COND_EXE   header;
    CONDITIONAL_EXECUTION_ADDR_LOW  dstAddrLo;
    CONDITIONAL_EXECUTION_ADDR_HIGH dstAddrHi;
} DMA_CMD_CONDITIONAL_EXECUTION;

/////////////////////////////////////////////////////////////////////////////////
// CONSTANT_FILL command
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
        unsigned int fillPattern     : 32;   // 32 bit value to fill destination with
    } bits;
    unsigned int u32All;
} CONSTANT_FILL_SOURCE_DATA;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int dstVirtAddrMode : 1;   // Signal dst addr is virtual, Ignored by DMA
        unsigned int                 : 1;   // reserved
        unsigned int dstAddrLo       : 30;  // Dst Address bits [31-2]
#else
        unsigned int dstAddrLo       : 30;  // Dst Address bits [31-2]
        unsigned int                 : 1;   // reserved// reserved
        unsigned int dstVirtAddrMode : 1;   // Signal dst addr is virt
#endif
    } bits;
    unsigned int u32All;
} CONSTANT_FILL_DST_ADDR_LOW;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int                 : 16;  // reserved
        unsigned int dstAddrHi       : 8;   // Dst Address bits [16-23]
        unsigned int                 : 8;   // reserved
#else
        unsigned int                 : 8;   // reserved
        unsigned int dstAddrHi       : 8;   // Dst Address bits [16-23]
        unsigned int                 : 16;  // reserved
#endif
    } bits;
    unsigned int u32All;
} CONSTANT_FILL_DST_ADDR_HIGH;

typedef struct _DMA_CMD_PACKET_CONSTANT_FILL
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;
    CONSTANT_FILL_DST_ADDR_LOW  dstAddrLo;
    CONSTANT_FILL_SOURCE_DATA   sourceData;
    CONSTANT_FILL_DST_ADDR_HIGH dstAddrHi;
} DMA_CMD_PACKET_CONSTANT_FILL;

/////////////////////////////////////////////////////////////////////////////////
// WRITE command
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int dstVirtAddrMode : 1;   // Signal dst addr is virtual, Ignored by DMA
        unsigned int                 : 1;   // reserved
        unsigned int dstAddrLo       : 30;  // Dst Address bits [31-2]
#else
        unsigned int dstAddrLo       : 30;  // Dst Address bits [31-2]
        unsigned int                 : 1;   // reserved// reserved
        unsigned int dstVirtAddrMode : 1;   // Signal dst addr is virt
#endif
    } bits;
    unsigned int u32All;
} WRITE_DST_ADDR_LOW;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int dstAddrHi       : 8;   // Dst Address bits [39-32]
        unsigned int dstSwapMode     : 2;   // Enable swap mode for dst
        unsigned int                 : 22;  // reserved
#else
        unsigned int                 : 22;  // reserved
        unsigned int dstSwapMode     : 2;   // Enable swap mode for dst
        unsigned int dstAddrHi       : 8;   // Dst Address bits [39-32]
#endif
    } bits;
    unsigned int u32All;
} WRITE_DST_ADDR_HIGH;

typedef struct _DMA_CMD_PACKET_WRITE
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;
    WRITE_DST_ADDR_LOW  dstAddrLo;
    WRITE_DST_ADDR_HIGH dstAddrHi;
} DMA_CMD_PACKET_WRITE;

/////////////////////////////////////////////////////////////////////////////////
// COPY command
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int dstVirtAddrMode : 1;   // Signal dst addr is virtual, Ignored by DMA
        unsigned int                 : 1;   // reserved
        unsigned int dstAddrLo       : 30;  // Dst Address bits [31-2]
#else
        unsigned int dstAddrLo       : 30;  // Dst Address bits [31-2]
        unsigned int                 : 1;   // reserved// reserved
        unsigned int dstVirtAddrMode : 1;   // Signal dst addr is virt
#endif
    } bits;
    unsigned int u32All;
} COPY_DST_ADDR_LOW;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int srcVirtAddrMode : 1;   // Signal src addr is virtual, Ignored by DMA
        unsigned int                 : 1;   // reserved
        unsigned int srcAddrLo       : 30;  // Src Address bits [31-2]
#else
        unsigned int srcAddrLo       : 30;  // Src Address bits [31-2]
        unsigned int                 : 1;   // reserved
        unsigned int srcVirtAddrMode : 1;   // Signal src addr is virtual, Ignored by DMA
#endif
    } bits;
    unsigned int u32All;
} COPY_SRC_ADDR_LOW;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int dstAddrHi       : 8;   // Dst Address bits [39-32]
        unsigned int dstSwapMode     : 2;   // Enable swap mode for dst
        unsigned int                 : 22;  // reserved
#else
        unsigned int                 : 22;  // reserved
        unsigned int dstSwapMode     : 2;   // Enable swap mode for dst
        unsigned int dstAddrHi       : 8;   // Dst Address bits [39-32]
#endif
    } bits;
    unsigned int u32All;
} COPY_DST_ADDR_HIGH;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int srcAddrHi       : 8;   // Src Address bits [39-32]
        unsigned int srcSwapMode     : 2;   // Enable swap mode for src
        unsigned int                 : 22;  // reserved
#else
        unsigned int                 : 22;  // reserved
        unsigned int srcSwapMode     : 2;   // Enable swap mode for src
        unsigned int srcAddrHi       : 8;   // Src Address bits [39-32]
#endif
    } bits;
    unsigned int u32All;
} COPY_SRC_ADDR_HIGH;

typedef struct _DMA_CMD_COPY
{
    DMA_CMD_HEADER header;
    COPY_DST_ADDR_LOW dstAddrLo;
    COPY_SRC_ADDR_LOW srcAddrLo;
    COPY_DST_ADDR_HIGH dstAddrHi;
    COPY_SRC_ADDR_HIGH srcAddrHi;
} DMA_CMD_COPY;

// Packet using common header definition
typedef struct _DMA_CMD_PACKET_COPY
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;
    COPY_DST_ADDR_LOW dstAddrLo;
    COPY_SRC_ADDR_LOW srcAddrLo;
    COPY_DST_ADDR_HIGH dstAddrHi;
    COPY_SRC_ADDR_HIGH srcAddrHi;
} DMA_CMD_PACKET_COPY;

/////////////////////////////////////////////////////////////////////////////////
// COPY_TILED command
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
        unsigned int tiledAddr          : 32;   // Tiled Address [39-8], 256-byte aligned
    } bits;
    unsigned int u32All;
} DMA_COPY_TILED_ADDR;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int pitchTileMax       : 10;   // Pitch Tile Max
        unsigned int heightTileMax      : 13;   // Height Max
        unsigned int                    : 1;    // reserved
        unsigned int pixel_size         : 3;    // Log2 bytes per pixel
        unsigned int array_mode         : 4;    // tile (or array) mode
        unsigned int direction          : 1;    // 0 = tiling, 1 = detiling
#else
        unsigned int direction          : 1;    // 0 = tiling, 1 = detiling
        unsigned int array_mode         : 4;    // tile (or array) mode
        unsigned int pixel_size         : 3;    // Log2 bytes per pixel
        unsigned int                    : 1;    // reserved
        unsigned int heightTileMax      : 13;   // Height Max
        unsigned int pitchTileMax       : 10;   // Pitch Tile Max
#endif
    } bits;
    unsigned int u32All;
} DMA_COPY_TILED_INFO_0;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int z                  : 11;   // Z coord start relative to surface base
        unsigned int                    : 1;    // reserved
        unsigned int sliceTileMax       : 20;   // Slice Tile Max
#else
        unsigned int sliceTileMax       : 20;   // Slice Tile Max
        unsigned int                    : 1;    // reserved
        unsigned int z                  : 11;   // Z coord start relative to surface base
#endif
    } bits;
    unsigned int u32All;
} DMA_COPY_TILED_INFO_1;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int tiledVirtAddrMode  : 1;    // Signal tiled addr is virtual, Ignored by DMA
        unsigned int tiledSwapMode      : 2;    // Enable swap mode for tiled surface
        unsigned int x                  : 13;   // X coord start relative to surface base
        unsigned int                    : 1;    // reserved
        unsigned int y                  : 13;   // Y coord start relative to surface base
        unsigned int                    : 2;    // reserved
#else
        unsigned int                    : 2;    // reserved
        unsigned int y                  : 13;   // Y coord start relative to surface base
        unsigned int                    : 1;    // reserved
        unsigned int x                  : 13;   // X coord start relative to surface base
        unsigned int tiledSwapMode      : 2;    // Enable swap mode for tiled surface
        unsigned int tiledVirtAddrMode  : 1;    // Signal tiled addr is virtual, Ignored by DMA
#endif
    } bits;
    unsigned int u32All;
} DMA_COPY_TILED_INFO_2;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int linearVirtAddrMode : 1;    // Signal linear addr is virtual, Ignored by DMA
        unsigned int                    : 1;    // reserved
        unsigned int linearAddrLo       : 30;   // Linear Address [31-2]
#else
        unsigned int linearAddrLo       : 30;   // Linear Address [31-2]
        unsigned int                    : 1;    // reserved
        unsigned int linearVirtAddrMode : 1;    // Signal linear addr is virtual, Ignored by DMA
#endif
    } bits;
    unsigned int u32All;
} DMA_COPY_LINEAR_ADDR_LOW;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int linearAddrHi       : 8;    // Linear Address [39-32]
        unsigned int linearSwapMode     : 2;    // Enable swap mode for linear surface
        unsigned int                    : 22;   // reserved
#else
        unsigned int                    : 22;   // reserved
        unsigned int linearSwapMode     : 2;    // Enable swap mode for linear surface
        unsigned int linearAddrHi       : 8;    // Linear Address [39-32]
#endif
    } bits;
    unsigned int u32All;
} DMA_COPY_LINEAR_ADDR_HIGH;

typedef struct _DMA_CMD_TILED_COPY
{
    DMA_CMD_HEADER header;
    DMA_COPY_TILED_ADDR tiledAddr;
    DMA_COPY_TILED_INFO_0 tiledInfo0;
    DMA_COPY_TILED_INFO_1 tiledInfo1;
    DMA_COPY_TILED_INFO_2 tiledInfo2;
    DMA_COPY_LINEAR_ADDR_LOW linearAddrLo;
    DMA_COPY_LINEAR_ADDR_HIGH linearAddrHi;
} DMA_CMD_TILED_COPY;

// Packet using common header definition
typedef struct _DMA_CMD_PACKET_TILED_COPY
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;
    DMA_COPY_TILED_ADDR tiledAddr;
    DMA_COPY_TILED_INFO_0 tiledInfo0;
    DMA_COPY_TILED_INFO_1 tiledInfo1;
    DMA_COPY_TILED_INFO_2 tiledInfo2;
    DMA_COPY_LINEAR_ADDR_LOW linearAddrLo;
    DMA_COPY_LINEAR_ADDR_HIGH linearAddrHi;
} DMA_CMD_PACKET_TILED_COPY;

/////////////////////////////////////////////////////////////////////////////////
// NOP command
/////////////////////////////////////////////////////////////////////////////////

typedef struct _DMA_CMD_NOP
{
    DMA_CMD_HEADER header;
} DMA_CMD_NOP;

// Packet using common header definition
typedef struct _DMA_CMD_PACKET_NOP
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;
} DMA_CMD_PACKET_NOP;

/////////////////////////////////////////////////////////////////////////////////
// INDIRECT_BUFFER command
// For R6xx and up the IB packet must end on an 8DW(256bit) boundary so
// base drivers(KMD/CMM) will include 4 extra NOPS when its used.
/////////////////////////////////////////////////////////////////////////////////

typedef struct _DMA_CMD_INDIRECT_BUFFER
{
    DMA_CMD_NOP nop;                 // Must be present because the packet MUST end on a 64 bit boundary
    DMA_CMD_HEADER header;

#if defined(LITTLEENDIAN_CPU)
    unsigned int  v             : 1;    // 0==physical Address, 1== virtual address
    unsigned int  reserved1     : 4;    // Set to zero
    unsigned int  ib_base_lo    : 27;   // bits 31:5
#else
    unsigned int  ib_base_lo    : 27;   // bits 31:5
    unsigned int  reserved1     : 4;    // Set to zero
    unsigned int  v             : 1;    // 0==physical Address, 1== virtual address
#endif

#if defined(LITTLEENDIAN_CPU)
    unsigned int  ib_base_hi    : 8;    // bits 39:32
    unsigned int  reserved2     : 8;    // Set to zero
    unsigned int  ib_size       : 16;
#else
    unsigned int  ib_size       : 16;
    unsigned int  reserved2     : 8;    // Set to zero
    unsigned int  ib_base_hi    : 8;    // bits 39:32
#endif

} DMA_CMD_INDIRECT_BUFFER;

/////////////////////////////////////////////////////////////////////////////////
// SEMAPHORE command
/////////////////////////////////////////////////////////////////////////////////

typedef struct _DMA_CMD_SEMAPHORE
{
    DMA_CMD_HEADER header;

#if defined(LITTLEENDIAN_CPU)
    unsigned int    reserved1     : 3;  // Set to zero
    unsigned int    sem_addr_low  : 29; // address bits 31:3
#else
    unsigned int    sem_addr_low  : 29; // address bits 31:3
    unsigned int    reserved1     : 3;  // Set to zero
#endif

#if defined(LITTLEENDIAN_CPU)
    unsigned int    sem_addr_high : 8;  // address bits 39:32
    unsigned int    reserved2     : 24; // Set to zero
#else
    unsigned int    reserved2     : 24; // Set to zero
    unsigned int    sem_addr_high : 8;  // address bits 39:32
#endif

} DMA_CMD_SEMAPHORE;

// Packet using common header definition
typedef struct _DMA_CMD_PACKET_SEMAPHORE
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;

#if defined(LITTLEENDIAN_CPU)
    unsigned int    reserved1     : 3;  // Set to zero
    unsigned int    sem_addr_low  : 29; // address bits 31:3
#else
    unsigned int    sem_addr_low  : 29; // address bits 31:3
    unsigned int    reserved1     : 3;  // Set to zero
#endif

#if defined(LITTLEENDIAN_CPU)
    unsigned int    sem_addr_high : 8;  // address bits 39:32
    unsigned int    reserved2     : 24; // Set to zero
#else
    unsigned int    reserved2     : 24; // Set to zero
    unsigned int    sem_addr_high : 8;  // address bits 39:32
#endif

} DMA_CMD_PACKET_SEMAPHORE;

/////////////////////////////////////////////////////////////////////////////////
// FENCE command
/////////////////////////////////////////////////////////////////////////////////

typedef struct _DMA_CMD_FENCE
{
    DMA_CMD_HEADER header;

#if defined(LITTLEENDIAN_CPU)
    unsigned int  v             : 1;    // 0==physical Address, 1== virtual address
    unsigned int  reserved1     : 1;    // Set to zero
    unsigned int  fence_base_lo : 30;   // bits 31:2
#else
    unsigned int  fence_base_lo : 30;   // bits 31:2
    unsigned int  reserved1     : 1;    // Set to zero
    unsigned int  v             : 1;    // 0==physical Address, 1== virtual address
#endif

#if defined(LITTLEENDIAN_CPU)
    unsigned int  fence_base_hi : 8;    // bits 39:32
    unsigned int  reserved2     : 24;   // Set to zero
#else
    unsigned int  reserved2     : 24;   // Set to zero
    unsigned int  fence_base_hi : 8;    // bits 39:32
#endif

    unsigned int  fence_data    : 32;   // data 31:0
} DMA_CMD_FENCE;

// Packet using common header definition
typedef struct _DMA_CMD_PACKET_FENCE
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;

#if defined(LITTLEENDIAN_CPU)
    unsigned int  v             : 1;    // 0==physical Address, 1== virtual address
    unsigned int  reserved1     : 1;    // Set to zero
    unsigned int  fence_base_lo : 30;   // bits 31:2
#else
    unsigned int  fence_base_lo : 30;   // bits 31:2
    unsigned int  reserved1     : 1;    // Set to zero
    unsigned int  v             : 1;    // 0==physical Address, 1== virtual address
#endif

#if defined(LITTLEENDIAN_CPU)
    unsigned int  fence_base_hi : 8;    // bits 39:32
    unsigned int  reserved2     : 24;   // Set to zero
#else
    unsigned int  reserved2     : 24;   // Set to zero
    unsigned int  fence_base_hi : 8;    // bits 39:32
#endif

    unsigned int  fence_data    : 32;   // data 31:0
} DMA_CMD_PACKET_FENCE;

/////////////////////////////////////////////////////////////////////////////////
// TRAP command
/////////////////////////////////////////////////////////////////////////////////

typedef struct _DMA_CMD_TRAP
{
    DMA_CMD_HEADER header;
} DMA_CMD_TRAP;

// Packet using common header definition
typedef struct _DMA_CMD_PACKET_TRAP
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;
} DMA_CMD_PACKET_TRAP;

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
// Evergreen-specific definitions:
// The following packets are either new to Evergreen, or are redefined
// due to the format of the packet header changing on Evergreen.
// Where the difference is only due to the packet header, such redefinitions
// may be removed if driver components switch to use the definitions that can
// be used on all types of GPUs, as noted.
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

#define DMA_OPCODE_SHIFT     28
#define DMA_R8XXCMD_SHIFT    26
#define DMA_TILE_SHIFT       23
#define DMA_SEMA_WAIT_SHIFT  22
#define DMA_COUNT_SHIFT      0

// This will need to be removed.
#define DMA_HEADER(opcode,tile,sema,count)                   \
    (((opcode) << DMA_OPCODE_SHIFT) |                        \
    ((tile) << DMA_TILE_SHIFT)      |                        \
    ((sema) << DMA_SEMA_WAIT_SHIFT) |                        \
    ((count) << DMA_COUNT_SHIFT))

#define DMA_HDR(opcode,r8xxcmd,tile,sema,count)              \
    (((opcode) << DMA_OPCODE_SHIFT)  |                       \
    ((r8xxcmd) << DMA_R8XXCMD_SHIFT) |                       \
    ((tile) << DMA_TILE_SHIFT)       |                       \
    ((sema) << DMA_SEMA_WAIT_SHIFT)  |                       \
    ((count) << DMA_COUNT_SHIFT))

/////////////////////////////////////////////////////////////////////////////////
// INDIRECT_BUFFER Evergreen command
// NOTE, this cannot be removed since ib_size is larger then in
// previous generations.
// For R6xx and up the IB packet must end on an 8DW(256bit) boundary so
// base drivers(KMD/CMM) will include 4 extra NOPS when its used.
/////////////////////////////////////////////////////////////////////////////////

typedef struct _DMA_CMD_INDIRECT_BUFFER_EVERGREEN
{
    DMA_CMD_NOP nop;                 // Must be present because the packet MUST end on a 64 bit boundary
    DMA_CMD_HEADER_EVERGREEN header;

#if defined(LITTLEENDIAN_CPU)
    unsigned int v              : 1;    // 0==physical Address, 1== virtual address
    unsigned int  reserved1     : 4;    // Set to zero
    unsigned int  ib_base_lo    : 27;   // bits 31:5
#else
    unsigned int  ib_base_lo    : 27;   // bits 31:5
    unsigned int  reserved1     : 4;    // Set to zero
    unsigned int v              : 1;    // 0==physical Address, 1== virtual address
#endif

#if defined(LITTLEENDIAN_CPU)
    unsigned int  ib_base_hi    : 8;    // bits 7:0
    unsigned int  reserved2     : 4;    // Set to zero
    unsigned int  ib_size       : 20;   // bits 31:12
#else
    unsigned int  ib_size       : 20;   // bits 31:12
    unsigned int  reserved2     : 4;    // Set to zero
    unsigned int  ib_base_hi    : 8;    // bits 7:0
#endif

} DMA_CMD_INDIRECT_BUFFER_EVERGREEN;

typedef struct _DMA_CMD_PACKET_INDIRECT_BUFFER
{
    DMA_CMD_NOP nop;                 // Must be present because the packet MUST end on a 256 bit boundary
    DMA_GPUSPECIFIC_PACKET_HEADER header;

#if defined(LITTLEENDIAN_CPU)
    unsigned int  v             : 1;    // 0==physical Address, 1== virtual address
    unsigned int  reserved1     : 4;    // Set to zero
    unsigned int  ib_base_lo    : 27;   // bits 31:5
#else
    unsigned int  ib_base_lo    : 27;   // bits 31:5
    unsigned int  reserved1     : 4;    // Set to zero
    unsigned int  v             : 1;    // 0==physical Address, 1== virtual address
#endif

#if defined(LITTLEENDIAN_CPU)
    unsigned int  ib_base_hi    : 8;    // bits 7:0
    unsigned int  reserved2     : 4;    // Set to zero
    unsigned int  ib_size       : 20;   // bits 31:12
#else
    unsigned int  ib_size       : 20;   // bits 31:12
    unsigned int  reserved2     : 4;    // Set to zero
    unsigned int  ib_base_hi    : 8;    // bits 7:0
#endif

} DMA_CMD_PACKET_INDIRECT_BUFFER;

/////////////////////////////////////////////////////////////////////////////////
// SEMAPHORE Evergreen command
// NOTE, this can be removed if all driver components switch to
// use DMA_CMD_PACKET_SEMAPHORE_EVERGREEN, which can use
// both header types.
/////////////////////////////////////////////////////////////////////////////////

typedef struct _DMA_CMD_SEMAPHORE_EVERGREEN
{
    DMA_CMD_HEADER_EVERGREEN header;

#if defined(LITTLEENDIAN_CPU)
    unsigned int    reserved1     : 3;  // Set to zero
    unsigned int    sem_addr_low  : 29; // address bits 31:3
#else
    unsigned int    sem_addr_low  : 29; // address bits 31:3
    unsigned int    reserved1     : 3;  // Set to zero
#endif

#if defined(LITTLEENDIAN_CPU)
    unsigned int    sem_addr_high : 8;  // address bits 39:32
    unsigned int    reserved2     : 24; // Set to zero
#else
    unsigned int    reserved2     : 24; // Set to zero
    unsigned int    sem_addr_high : 8;  // address bits 39:32
#endif

} DMA_CMD_SEMAPHORE_EVERGREEN;

/////////////////////////////////////////////////////////////////////////////////
// LINEAR_DWORD_COPY Evergreen command
// NOTE, this can be removed if all driver components switch to
// use DMA_CMD_PACKET_COPY, which can use
// both header types.
/////////////////////////////////////////////////////////////////////////////////

// Same as DMA_CMD_COPY(used by R6xx/R7xx except it uses DMA_CMD_HEADER_EVERGREEN)
typedef struct _DMA_CMD_LINEAR_DWORD_COPY
{
    DMA_CMD_HEADER_EVERGREEN Header;
    COPY_DST_ADDR_LOW dstAddrLo;
    COPY_SRC_ADDR_LOW srcAddrLo;
    COPY_DST_ADDR_HIGH dstAddrHi;
    COPY_SRC_ADDR_HIGH srcAddrHi;
} DMA_CMD_LINEAR_DWORD_COPY;

/////////////////////////////////////////////////////////////////////////////////
// LINEAR_BYTE_COPY Evergreen command
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int linearAddrLo       : 32;   // Linear Address [31-0]
#else
        unsigned int linearAddrLo       : 32;   // Linear Address [31-0]
#endif
    } bits;
    unsigned int u32All;
} DMA_COPY_LINEAR_ADDR_LOW_EVERGREEN;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int linearAddrHi       : 8;    // Linear Address [39-32]
        unsigned int linearSwapMode     : 2;    // Enable swap mode for linear surface
        unsigned int linearVirtAddrMode : 1;    // Signal linear addr is virtual, Ignored by DMA
        unsigned int                    : 21;   // reserved
#else
        unsigned int                    : 21;   // reserved
        unsigned int linearVirtAddrMode : 1;    // Signal linear addr is virtual, Ignored by DMA
        unsigned int linearSwapMode     : 2;    // Enable swap mode for linear surface
        unsigned int linearAddrHi       : 8;    // Linear Address [39-32]
#endif
    } bits;
    unsigned int u32All;
} DMA_COPY_LINEAR_ADDR_HIGH_EVERGREEN;

// New packet for Evergreen
typedef struct _DMA_CMD_LINEAR_BYTE_COPY
{
    DMA_CMD_HEADER_EVERGREEN             header;
    DMA_COPY_LINEAR_ADDR_LOW_EVERGREEN   dstAddrLo;
    DMA_COPY_LINEAR_ADDR_LOW_EVERGREEN   srcAddrLo;
    DMA_COPY_LINEAR_ADDR_HIGH_EVERGREEN  dstAddrHi;
    DMA_COPY_LINEAR_ADDR_HIGH_EVERGREEN  srcAddrHi;
} DMA_CMD_LINEAR_BYTE_COPY;

// Packet using common header definition
typedef struct _DMA_CMD_PACKET_LINEAR_BYTE_COPY
{
    DMA_GPUSPECIFIC_PACKET_HEADER        header;
    DMA_COPY_LINEAR_ADDR_LOW_EVERGREEN   dstAddrLo;
    DMA_COPY_LINEAR_ADDR_LOW_EVERGREEN   srcAddrLo;
    DMA_COPY_LINEAR_ADDR_HIGH_EVERGREEN  dstAddrHi;
    DMA_COPY_LINEAR_ADDR_HIGH_EVERGREEN  srcAddrHi;
} DMA_CMD_PACKET_LINEAR_BYTE_COPY;

/////////////////////////////////////////////////////////////////////////////////
// COPY_L2TT2L Evergreen command
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
        unsigned int baseaddr          : 32;   // Tiled Address [39-8], 256-byte aligned
    } bits;
    unsigned int u32All;
} DMA_COPY_L2TT2L_ADDR;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int reserved1          : 16;   // reserved
        unsigned int mtaspect           : 2;    // macro tile aspect ratio
        unsigned int bankwidth          : 2;    // bank width
        unsigned int reserved2          : 1;    // reserved
        unsigned int bankheight         : 2;    // bank height
        unsigned int reserved3          : 1;    // reserved
        unsigned int pixel_size         : 3;    // Log2 bytes per pixel
        unsigned int array_mode         : 4;    // tile (or array) mode
        unsigned int direction          : 1;    // 0 = tiling, 1 = detiling
#else
        unsigned int direction          : 1;    // 0 = tiling, 1 = detiling
        unsigned int array_mode         : 4;    // tile (or array) mode
        unsigned int pixel_size         : 3;    // Log2 bytes per pixel
        unsigned int reserved3          : 1;    // reserved
        unsigned int bankheight         : 2;    // bank height
        unsigned int reserved2          : 1;    // reserved
        unsigned int bankwidth          : 2;    // bank width
        unsigned int mtaspect           : 2;    // macro tile aspect ratio
        unsigned int reserved1          : 16;   // reserved
#endif
    } bits;
    unsigned int u32All;
} DMA_COPY_L2TT2L_INFO_0;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int pitchTileMax       : 11;   // per the CB register spec
        unsigned int reserved1          : 5;    // reserved
        unsigned int heightMax          : 14;   // Height_Max indicates height-1 of tiled surface. The unit is pixel level.
        unsigned int reserved2          : 2;    // reserved
#else
        unsigned int reserved2          : 2;    // reserved
        unsigned int heightMax          : 14;   // Height_Max indicates height-1 of tiled surface. The unit is pixel level.
        unsigned int reserved           : 5;    // reserved
        unsigned int pitchTileMax       : 11;   // per the CB register spec
#endif
    } bits;
    unsigned int u32All;
} DMA_COPY_L2TT2L_INFO_1;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int sliceTileMax       : 22;   // Slice Tile Max
        unsigned int reserved           : 10;   // reserved
#else
        unsigned int reserved           : 10;   // reserved
        unsigned int sliceTileMax       : 22;   // Slice Tile Max
#endif
    } bits;
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int sliceTileMax       : 22;   // Slice Tile Max
        unsigned int reserved           : 4;
        unsigned int pipe_config        : 5;
        unsigned int reserved1          : 1;
#else
        unsigned int reserved1          : 1;
        unsigned int pipe_config        : 5;
        unsigned int reserved           : 4;
        unsigned int sliceTileMax       : 22;   // Slice Tile Max
#endif
    } si_bits;
    unsigned int u32All;
} DMA_COPY_L2TT2L_INFO_2;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int x                  : 14;   // X coord start relative to surface base
        unsigned int                    : 4;    // reserved
        unsigned int z                  : 11;   // Z coord start relative to surface base
        unsigned int                    : 3;    // reserved
#else
        unsigned int                    : 3;    // reserved
        unsigned int z                  : 11;   // Z coord start relative to surface base
        unsigned int                    : 4;    // reserved
        unsigned int x                  : 14;   // X coord start relative to surface base
#endif
    } bits;
    unsigned int u32All;
} DMA_COPY_L2TT2L_INFO_3;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int y                  : 14;   // Y coord start relative to surface base
        unsigned int reserved1          : 7;    // reserved
        unsigned int tilesplit          : 3;    // number of bytes that will be stored contiguously for each tile
        unsigned int reserved2          : 1;    // reserved
        unsigned int numbank            : 2;    // number of memory banks for tiling purposes.
        unsigned int reserved3          : 1;    // reserved
        unsigned int nd                 : 1;    // ND(non-display)indicates if tile uses non-displayable tiling order
        unsigned int v                  : 1;    // indicates a virtual address using the PDMA's page table.  If "v" is 0 it indicates a physical address.
        unsigned int sw                 : 2;    // indicates swapping  mode used when enabled
#else
        unsigned int sw                 : 2;    // indicates swapping  mode used when enabled
        unsigned int v                  : 1;    // indicates a virtual address using the PDMA's page table.  If "v" is 0 it indicates a physical address.
        unsigned int nd                 : 1;    // ND(non-display)indicates if tile uses non-displayable tiling order
        unsigned int reserved3          : 1;    // reserved
        unsigned int numbank            : 2;    // number of memory banks for tiling purposes.
        unsigned int reserved2          : 1;    // reserved
        unsigned int tilesplit          : 3;    // number of bytes that will be stored contiguously for each tile
        unsigned int reserved1          : 7;    // reserved
        unsigned int y                  : 14;   // Y coord start relative to surface base
#endif
    } bits;
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int y                  : 14;   // Y coord start relative to surface base
        unsigned int reserved1          : 7;    // reserved
        unsigned int tilesplit          : 3;    // number of bytes that will be stored contiguously for each tile
        unsigned int reserved2          : 1;    // reserved
        unsigned int numbank            : 2;    // number of memory banks for tiling purposes.
        unsigned int mtMode             : 2;    // MT mode
        unsigned int reserved3          : 1;    // reserved
        unsigned int sw                 : 2;    // indicates swapping  mode used when enabled
#else
        unsigned int sw                 : 2;    // indicates swapping  mode used when enabled
        unsigned int reserved3          : 1;    // reserved
        unsigned int mtMode             : 2;    // MT mode
        unsigned int numbank            : 2;    // number of memory banks for tiling purposes.
        unsigned int reserved2          : 1;    // reserved
        unsigned int tilesplit          : 3;    // number of bytes that will be stored contiguously for each tile
        unsigned int reserved1          : 7;    // reserved
        unsigned int y                  : 14;   // Y coord start relative to surface base
#endif
    } si_bits;
    unsigned int u32All;
} DMA_COPY_L2TT2L_INFO_4;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int v                  : 1;    // Signal linear addr is virtual, Ignored by DMA
        unsigned int o                  : 1;    // reserved
        unsigned int linearAddrLo       : 30;   // Linear Address [31-2]
#else
        unsigned int linearAddrLo       : 30;   // Linear Address [31-2]
        unsigned int o                  : 1;    // reserved
        unsigned int v                  : 1;    // Signal linear addr is virtual, Ignored by DMA
#endif
    } bits;
    unsigned int u32All;
} DMA_COPY_L2TT2L_ADDR_LOW;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int linearAddrHi       : 8;    // Linear Address [39-32]
        unsigned int reserved           : 22;   // reserved
        unsigned int linearSwapMode     : 2;    // Enable swap mode for linear surface
#else
        unsigned int linearSwapMode     : 2;    // Enable swap mode for linear surface
        unsigned int reserved           : 22;   // reserved
        unsigned int linearAddrHi       : 8;    // Linear Address [39-32]
#endif
    } bits;
    unsigned int u32All;
} DMA_COPY_L2TT2L_ADDR_HIGH;

typedef struct _DMA_CMD_L2TT2L_COPY
{
    DMA_CMD_HEADER_EVERGREEN header;
    DMA_COPY_L2TT2L_ADDR tiledAddr;
    DMA_COPY_L2TT2L_INFO_0 tiledInfo0;
    DMA_COPY_L2TT2L_INFO_1 tiledInfo1;
    DMA_COPY_L2TT2L_INFO_2 tiledInfo2;
    DMA_COPY_L2TT2L_INFO_3 tiledInfo3;
    DMA_COPY_L2TT2L_INFO_4 tiledInfo4;
    DMA_COPY_L2TT2L_ADDR_LOW linearAddrLow;
    DMA_COPY_L2TT2L_ADDR_HIGH linearAddrHi;
} DMA_CMD_L2TT2L_COPY;

// Packet using common header definition
typedef struct _DMA_CMD_PACKET_L2TT2L_COPY
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;
    DMA_COPY_L2TT2L_ADDR tiledAddr;
    DMA_COPY_L2TT2L_INFO_0 tiledInfo0;
    DMA_COPY_L2TT2L_INFO_1 tiledInfo1;
    DMA_COPY_L2TT2L_INFO_2 tiledInfo2;
    DMA_COPY_L2TT2L_INFO_3 tiledInfo3;
    DMA_COPY_L2TT2L_INFO_4 tiledInfo4;
    DMA_COPY_L2TT2L_ADDR_LOW linearAddrLow;
    DMA_COPY_L2TT2L_ADDR_HIGH linearAddrHi;
} DMA_CMD_PACKET_L2TT2L_COPY;

/////////////////////////////////////////////////////////////////////////////////
// SRBM WRITE command
/////////////////////////////////////////////////////////////////////////////////

// New packet for Evergreen
typedef struct _DMA_CMD_PACKET_SRBM_WRITE
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;
#if defined(LITTLEENDIAN_CPU)
    unsigned int  address       : 16;   // register address
    unsigned int  byteenable    : 4;    // indicates byte enable
    unsigned int  reserved1     : 12;   // Set to zero
#else
    unsigned int  reserved1     : 12;   // Set to zero
    unsigned int  byteenable    : 4;    // indicates byte enable
    unsigned int  address       : 16;   // register address
#endif

#if defined(LITTLEENDIAN_CPU)
    unsigned int  data          : 32;   // register data
#else
    unsigned int  data          : 32;   // register data
#endif

} DMA_CMD_PACKET_SRBM_WRITE;

// New SRBM read packet
typedef struct _DMA_CMD_PACKET_SRBM_READ
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;
#if defined(LITTLEENDIAN_CPU)
    unsigned int  address       : 16;   // register address
    unsigned int  reserved1     : 4;    // reserved bits
    unsigned int  retry_count   : 12;   // Retry Count
#else
    unsigned int  retry_count   : 12;   // Retry Count
    unsigned int  reserved1     : 4;    // Reserved bits
    unsigned int  address       : 16;   // register address
#endif

    unsigned int  mask          : 32;   // register mask
    unsigned int  value         : 32;   // register data

} DMA_CMD_PACKET_SRBM_READ;

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
// Cayman-specific definitions:
// The following packets are either new to Cayman, or are redefined
// due to the format of the packet header changing on Evergreen.
// Where the difference is only due to the packet header, such redefinitions
// may be removed if driver components switch to use the definitions that can
// be used on all types of GPUs, as noted.
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// Copy L2L DWORD Cayman command
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int dstAddrHi       : 8;   // Dst Address bits [39-32]
        unsigned int dstSwapMode     : 2;   // Enable swap mode for dst
        unsigned int                 : 21;  // reserved
        unsigned int c               : 1;   // CRC enable
#else
        unsigned int c               : 1;   // CRC enable
        unsigned int                 : 21;  // reserved
        unsigned int dstSwapMode     : 2;   // Enable swap mode for dst
        unsigned int dstAddrHi       : 8;   // Dst Address bits [39-32]
#endif
    } bits;
    unsigned int u32All;
} COPY_DST_ADDR_HIGH_CAYMAN;

typedef struct _DMA_CMD_PACKET_L2L_DWORD_COPY
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;
    COPY_DST_ADDR_LOW dstAddrLo;
    COPY_SRC_ADDR_LOW srcAddrLo;
    COPY_DST_ADDR_HIGH_CAYMAN dstAddrHi;
    COPY_SRC_ADDR_HIGH srcAddrHi;
} DMA_CMD_PACKET_L2L_DWORD_COPY;

/////////////////////////////////////////////////////////////////////////////////
// COPY L2L Partial Cayman command
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int                    : 2;
        unsigned int addrLo             : 30;
#else
        unsigned int addrLo             : 30;
        unsigned int                    : 2;
#endif
    } bits;
    unsigned int u32All;

} DMA_COPY_LINEAR_PARTIAL_ADDR_LO;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int addrHi             : 8;
        unsigned int sw                 : 2;
        unsigned int v                  : 1;
        unsigned int                    : 2;
        unsigned int pitch              : 19;
#else
        unsigned int pitch              : 19;
        unsigned int                    : 2;
        unsigned int v                  : 1;
        unsigned int sw                 : 2;
        unsigned int addrHi             : 8;
#endif
    } bits;
    unsigned int u32All;

} DMA_COPY_LINEAR_PARTIAL_ADDR_HI;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int dX                 : 14;
        unsigned int                    : 2;
        unsigned int dY                 : 14;
        unsigned int                    : 2;
#else
        unsigned int                    : 2;
        unsigned int dY                 : 14;
        unsigned int                    : 2;
        unsigned int dX                 : 14;
#endif
    } bits;
    unsigned int u32All;

} DMA_COPY_LINEAR_PARTIAL_SIZE_X_Y;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int dZ                 : 11;
        unsigned int                    : 18;
        unsigned int size               : 3;
#else
        unsigned int size               : 3;
        unsigned int                    : 18;
        unsigned int dZ                 : 11;
#endif
    } bits;
    unsigned int u32All;

} DMA_COPY_LINEAR_PARTIAL_SIZE_Z;

typedef struct _DMA_CMD_PACKET_L2L_PARTIAL_COPY
{
    DMA_GPUSPECIFIC_PACKET_HEADER        header;
    DMA_COPY_LINEAR_PARTIAL_ADDR_LO      srcAddrLo;
    DMA_COPY_LINEAR_PARTIAL_ADDR_HI      srcAddrHi;
    unsigned int                            srcSlicePitch;
    DMA_COPY_LINEAR_PARTIAL_ADDR_LO      dstAddrLo;
    DMA_COPY_LINEAR_PARTIAL_ADDR_HI      dstAddrHi;
    unsigned int                            dstSlicePitch;
    DMA_COPY_LINEAR_PARTIAL_SIZE_X_Y     sizeXY;
    DMA_COPY_LINEAR_PARTIAL_SIZE_Z       sizeZ;

} DMA_CMD_PACKET_L2L_PARTIAL_COPY;

/////////////////////////////////////////////////////////////////////////////////
// COPY L2T/T2L Partial Cayman command
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int y                  : 14;   // Y coord start relative to surface base
        unsigned int                    : 7;    // reserved
        unsigned int tilesplit          : 3;    // number of bytes that will be stored contiguously for each tile
        unsigned int                    : 1;    // reserved
        unsigned int numbank            : 2;    // number of memory banks for tiling purposes.
        unsigned int nd                 : 2;    // ND(non-display)indicates if tile uses non-displayable tiling order
        unsigned int v                  : 1;    // indicates a virtual address using the PDMA's page table.  If "v" is 0 it indicates a physical address.
        unsigned int sw                 : 2;    // indicates swapping  mode used when enabled
#else
        unsigned int sw                 : 2;    // indicates swapping  mode used when enabled
        unsigned int v                  : 1;    // indicates a virtual address using the PDMA's page table.  If "v" is 0 it indicates a physical address.
        unsigned int nd                 : 2;    // ND(non-display)indicates if tile uses non-displayable tiling order
        unsigned int numbank            : 2;    // number of memory banks for tiling purposes.
        unsigned int                    : 1;    // reserved
        unsigned int tilesplit          : 3;    // number of bytes that will be stored contiguously for each tile
        unsigned int                    : 7;    // reserved
        unsigned int y                  : 14;   // Y coord start relative to surface base
#endif
    } bits;

    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int y                  : 14;   // Y coord start relative to surface base
        unsigned int                    : 7;    // reserved
        unsigned int tilesplit          : 3;    // number of bytes that will be stored contiguously for each tile
        unsigned int                    : 1;    // reserved
        unsigned int numbank            : 2;    // number of memory banks for tiling purposes.
        unsigned int mtmode             : 2;    // MT Mode
        unsigned int                    : 1;    // reserved
        unsigned int sw                 : 2;    // indicates swapping  mode used when enabled
#else
        unsigned int sw                 : 2;    // indicates swapping  mode used when enabled
        unsigned int                    : 1;    // reserved
        unsigned int mtmode             : 2;    // MT Mode
        unsigned int numbank            : 2;    // number of memory banks for tiling purposes.
        unsigned int                    : 1;    // reserved
        unsigned int tilesplit          : 3;    // number of bytes that will be stored contiguously for each tile
        unsigned int                    : 7;    // reserved
        unsigned int y                  : 14;   // Y coord start relative to surface base
#endif
    } si_bits;
    unsigned int u32All;
} DMA_COPY_L2TT2L_PARTIAL_INFO_4;

typedef struct _DMA_CMD_PACKET_L2TT2L_PARTIAL_COPY
{
    DMA_GPUSPECIFIC_PACKET_HEADER        header;
    DMA_COPY_L2TT2L_ADDR                 tiledAddr;
    DMA_COPY_L2TT2L_INFO_0               tiledInfo0;
    DMA_COPY_L2TT2L_INFO_1               tiledInfo1;
    DMA_COPY_L2TT2L_INFO_2               tiledInfo2;
    DMA_COPY_L2TT2L_INFO_3               tiledInfo3;
    DMA_COPY_L2TT2L_PARTIAL_INFO_4       tiledInfo4;
    DMA_COPY_LINEAR_PARTIAL_ADDR_LO      linearAddrLo;
    DMA_COPY_LINEAR_PARTIAL_ADDR_HI      linearAddrHi;
    unsigned int                            linearPitch;
    DMA_COPY_LINEAR_PARTIAL_SIZE_X_Y     sizeXY;
    DMA_COPY_LINEAR_PARTIAL_SIZE_Z       sizeZ;

} DMA_CMD_PACKET_L2TT2L_PARTIAL_COPY;

/////////////////////////////////////////////////////////////////////////////////
// COPY T2T Partial Cayman command
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int v0                 : 1;    // indicates a virtual address using the PDMA's page table.  If "v" is 0 it indicates a physical address.
        unsigned int sw0                : 2;    // indicates swapping  mode used when enabled
        unsigned int tilesplit          : 3;    // number of bytes that will be stored contiguously for each tile
        unsigned int                    : 1;    // reserved
        unsigned int numbank            : 2;    // number of memory banks for tiling purposes.
        unsigned int nd                 : 2;    // ND(non-display)indicates if tile uses non-displayable tiling order
        unsigned int v1                 : 1;    // indicates a virtual address using the PDMA's page table.  If "v" is 0 it indicates a physical address.
        unsigned int sw1                : 2;    // indicates swapping  mode used when enabled
        unsigned int                    : 2;
        unsigned int mtaspect           : 2;    // macro tile aspect ratio
        unsigned int bankwidth          : 2;    // bank width
        unsigned int                    : 1;    // reserved
        unsigned int bankheight         : 2;    // bank height
        unsigned int                    : 1;    // reserved
        unsigned int pixel_size         : 3;    // Log2 bytes per pixel
        unsigned int array_mode         : 4;    // tile (or array) mode
        unsigned int direction          : 1;    // 0 = tiling, 1 = detiling
#else
        unsigned int direction          : 1;    // 0 = tiling, 1 = detiling
        unsigned int array_mode         : 4;    // tile (or array) mode
        unsigned int pixel_size         : 3;    // Log2 bytes per pixel
        unsigned int                    : 1;    // reserved
        unsigned int bankheight         : 2;    // bank height
        unsigned int                    : 1;    // reserved
        unsigned int bankwidth          : 2;    // bank width
        unsigned int mtaspect           : 2;    // macro tile aspect ratio
        unsigned int                    : 2;
        unsigned int sw1                : 2;    // indicates swapping  mode used when enabled
        unsigned int v1                 : 1;    // indicates a virtual address using the PDMA's page table.  If "v" is 0 it indicates a physical address.
        unsigned int nd                 : 2;    // ND(non-display)indicates if tile uses non-displayable tiling order
        unsigned int numbank            : 2;    // number of memory banks for tiling purposes.
        unsigned int                    : 1;    // reserved
        unsigned int tilesplit          : 3;    // number of bytes that will be stored contiguously for each tile
        unsigned int sw0                : 2;    // indicates swapping  mode used when enabled
        unsigned int v0                 : 1;    // indicates a virtual address using the PDMA's page table.  If "v" is 0 it indicates a physical address.
#endif
    } bits;

    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int                    : 1;    //
        unsigned int sw0                : 2;    // indicates swapping  mode used when enabled
        unsigned int tilesplit          : 3;    // number of bytes that will be stored contiguously for each tile
        unsigned int                    : 1;    // reserved
        unsigned int numbank            : 2;    // number of memory banks for tiling purposes.
        unsigned int mt_mode            : 2;    // SI+ MT Mode
        unsigned int                    : 1;
        unsigned int sw1                : 2;    // indicates swapping  mode used when enabled
        unsigned int                    : 2;
        unsigned int mtaspect           : 2;    // macro tile aspect ratio
        unsigned int bankwidth          : 2;    // bank width
        unsigned int                    : 1;    // reserved
        unsigned int bankheight         : 2;    // bank height
        unsigned int                    : 1;    // reserved
        unsigned int pixel_size         : 3;    // Log2 bytes per pixel
        unsigned int array_mode         : 4;    // tile (or array) mode
        unsigned int direction          : 1;    // 0 = tiling, 1 = detiling
#else
        unsigned int direction          : 1;    // 0 = tiling, 1 = detiling
        unsigned int array_mode         : 4;    // tile (or array) mode
        unsigned int pixel_size         : 3;    // Log2 bytes per pixel
        unsigned int                    : 1;    // reserved
        unsigned int bankheight         : 2;    // bank height
        unsigned int                    : 1;    // reserved
        unsigned int bankwidth          : 2;    // bank width
        unsigned int mtaspect           : 2;    // macro tile aspect ratio
        unsigned int                    : 2;
        unsigned int sw1                : 2;    // indicates swapping  mode used when enabled
        unsigned int                    : 1;
        unsigned int mt_mode            : 2;    // SI+ MT Mode
        unsigned int numbank            : 2;    // number of memory banks for tiling purposes.
        unsigned int                    : 1;    // reserved
        unsigned int tilesplit          : 3;    // number of bytes that will be stored contiguously for each tile
        unsigned int sw0                : 2;    // indicates swapping  mode used when enabled
        unsigned int                    : 1;    //
#endif
    } si_bits;
    unsigned int u32All;
} DMA_COPY_T2T_INFO_0;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int dst                : 11;
        unsigned int                    : 2;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int src                : 11;
        unsigned int                    : 2;
#else
        unsigned int                    : 2;
        unsigned int src                : 11;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int                    : 2;
        unsigned int dst                : 11;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int                    : 1;
#endif
    } bits;
    unsigned int u32All;

} DMA_COPY_T2T_INFO_1;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int dstz               : 8;
        unsigned int                    : 5;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int srcz               : 8;
        unsigned int                    : 5;
#else
        unsigned int                    : 5;
        unsigned int srcz               : 8;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int                    : 5;
        unsigned int dstz               : 8;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int                    : 1;
#endif
    } bits;

    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int dstz               : 11;
        unsigned int                    : 5;
        unsigned int srcz               : 11;
        unsigned int                    : 5;
#else
        unsigned int                    : 5;
        unsigned int srcz               : 11;
        unsigned int                    : 5;
        unsigned int dstz               : 11;
#endif
    } si_bits;
    unsigned int u32All;

} DMA_COPY_T2T_INFO_2;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int dz                 : 8;
        unsigned int                    : 21;
#else
        unsigned int                    : 21;
        unsigned int dz                 : 8;
        unsigned int                    : 1;
        unsigned int                    : 1;
        unsigned int                    : 1;
#endif
    } bits;

    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int dz                 : 11;
        unsigned int                    : 21;
#else
        unsigned int                    : 21;
        unsigned int dz                 : 11;

#endif
    } si_bits;
    unsigned int u32All;

} DMA_COPY_T2T_INFO_3;

typedef struct _DMA_CMD_PACKET_T2T_PARTIAL_COPY
{
    DMA_GPUSPECIFIC_PACKET_HEADER        header;
    DMA_COPY_L2TT2L_ADDR                 srcAddr;
    DMA_COPY_L2TT2L_INFO_1               srcInfo1;
    DMA_COPY_L2TT2L_INFO_2               srcInfo2;
    DMA_COPY_L2TT2L_ADDR                 dstAddr;
    DMA_COPY_L2TT2L_INFO_1               dstInfo1;
    DMA_COPY_L2TT2L_INFO_2               dstInfo2;
    DMA_COPY_T2T_INFO_0                  info0;
    DMA_COPY_T2T_INFO_1                  xInfo1;
    DMA_COPY_T2T_INFO_1                  yInfo1;
    DMA_COPY_T2T_INFO_2                  zInfo2;
    DMA_COPY_T2T_INFO_1                  dInfo1;
    DMA_COPY_T2T_INFO_3                  dzInfo3;

} DMA_CMD_PACKET_T2T_PARTIAL_COPY;

/////////////////////////////////////////////////////////////////////////////////
// COPY L2S/S2L Cayman command
/////////////////////////////////////////////////////////////////////////////////

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int                    : 2;
        unsigned int addrLo             : 30;
#else
        unsigned int addrLo             : 30;
        unsigned int                    : 2;
#endif
    } bits;
    unsigned int u32All;

} DMA_COPY_L2SS2L_ADDR_LO;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int addrHi             : 8;
        unsigned int                    : 2;
        unsigned int stride             : 11;
        unsigned int                    : 3;
        unsigned int startIndex         : 4;
        unsigned int v                  : 1;
        unsigned int sw                 : 2;
        unsigned int d                  : 1;
#else
        unsigned int d                  : 1;
        unsigned int sw                 : 2;
        unsigned int v                  : 1;
        unsigned int startIndex         : 4;
        unsigned int                    : 3;
        unsigned int stride             : 11;
        unsigned int                    : 2;
        unsigned int addrHi             : 8;
#endif
    } bits;
    unsigned int u32All;

} DMA_COPY_L2SS2L_STRUCTURE_INFO;

typedef union
{
    struct
    {
#if defined(LITTLEENDIAN_CPU)
        unsigned int addrHi             : 8;
        unsigned int sw                 : 2;
        unsigned int v                  : 1;
        unsigned int                    : 21;
#else
        unsigned int                    : 21;
        unsigned int v                  : 1;
        unsigned int sw                 : 2;
        unsigned int addrHi             : 8;
#endif
    } bits;
    unsigned int u32All;

} DMA_COPY_L2SS2L_ADDR_HI;

typedef struct _DMA_CMD_PACKET_L2SS2L_COPY
{
    DMA_GPUSPECIFIC_PACKET_HEADER        header;
    DMA_COPY_L2SS2L_ADDR_LO              sbufferAddr;
    DMA_COPY_L2SS2L_STRUCTURE_INFO       structInfo;
    unsigned int                         countIndex;
    DMA_COPY_L2SS2L_ADDR_LO              linearAddr;
    DMA_COPY_L2SS2L_ADDR_HI              linearAddrHi;

} DMA_CMD_PACKET_L2SS2L_COPY;

/////////////////////////////////////////////////////////////////////////////////
// New PTE write packet for Cayman
/////////////////////////////////////////////////////////////////////////////////

typedef struct _DMA_CMD_PACKET_WRITE_PTE
{
    DMA_GPUSPECIFIC_PACKET_HEADER header;
    WRITE_DST_ADDR_LOW  dstAddrLo;
    WRITE_DST_ADDR_HIGH dstAddrHi;
    unsigned int        maskLo;
    unsigned int        maskHi;
    unsigned int        initValueLo;
    unsigned int        initValueHi;
    unsigned int        incrementLo;
    unsigned int        incrementHi;
} DMA_CMD_PACKET_WRITE_PTE;

typedef struct _DMA_CMD_PACKET_POLL_REG_MEM
{
    unsigned int    count         : 20;    // count in DWORDs
    unsigned int    reserved      : 7;     // reserved
    unsigned int    mem           : 1;     // Memory or register space poll
    unsigned int    type          : 4;     // DMA_HEADER_TYPE

    unsigned int    reserved0     : 2;
    unsigned int    addr_lo       : 30;   // Addr [31:2]

    unsigned int    addr_hi       : 8;    // Addr [39:32]
    unsigned int    reserve1      : 8;
    unsigned int    retry_count   : 12;   // Retry count
    unsigned int    reserve2      : 4;

    unsigned int    mask;                 // Mask [31:0]
    unsigned int    reference;            // Reference [31:0]

    unsigned int    poll_interval : 16;
    unsigned int    reserve3      : 12;
    unsigned int    func          : 3;
    unsigned int    reserve4      : 1;

} DMA_CMD_PACKET_POLL_REG_MEM;

/////////////////////////////////////////////////////////////////////////////////
// Command packet size definitions
/////////////////////////////////////////////////////////////////////////////////

#define DMA_CMD_COPY_DWORDS                                  \
    (sizeof(DMA_CMD_COPY) / sizeof(unsigned int))

#define DMA_CMD_TILED_COPY_DWORDS                            \
    (sizeof(DMA_CMD_TILED_COPY) / sizeof(unsigned int))

#define DMA_CMD_LINEAR_DWORD_COPY_DWORDS                     \
    (sizeof(DMA_CMD_LINEAR_DWORD_COPY) / sizeof(unsigned int))

#define DMA_CMD_LINEAR_BYTE_COPY_DWORDS                      \
    (sizeof(DMA_CMD_LINEAR_BYTE_COPY) / sizeof(unsigned int))

#define DMA_CMD_L2TT2L_COPY_DWORDS                           \
    (sizeof(DMA_CMD_L2TT2L_COPY) / sizeof(unsigned int))

#define DMA_CMD_NOP_DWORDS                                   \
    (sizeof(DMA_CMD_NOP) / sizeof(unsigned int))

#define DMA_CMD_CONDITIONAL_EXECUTION_DWORDS                 \
    (sizeof(DMA_CMD_CONDITIONAL_EXECUTION) / sizeof(unsigned int))

/////////////////////////////////////////////////////////////////////////////////
// Command packet size definitions for packets using common header definition
/////////////////////////////////////////////////////////////////////////////////

#define DMA_CMD_PACKET_CONSTANT_FILL_DWORDS                         \
    (sizeof(DMA_CMD_PACKET_CONSTANT_FILL) / sizeof(unsigned int))

#define DMA_CMD_PACKET_WRITE_DWORDS                                 \
    (sizeof(DMA_CMD_PACKET_WRITE) / sizeof(unsigned int))

#define DMA_CMD_PACKET_COPY_DWORDS                                  \
    (sizeof(DMA_CMD_PACKET_COPY) / sizeof(unsigned int))

#define DMA_CMD_PACKET_TILED_COPY_DWORDS                            \
    (sizeof(DMA_CMD_PACKET_TILED_COPY) / sizeof(unsigned int))

#define DMA_CMD_PACKET_LINEAR_DWORD_COPY_DWORDS                     \
    (sizeof(DMA_CMD_PACKET_LINEAR_DWORD_COPY) / sizeof(unsigned int))

#define DMA_CMD_PACKET_LINEAR_BYTE_COPY_DWORDS                      \
    (sizeof(DMA_CMD_PACKET_LINEAR_BYTE_COPY) / sizeof(unsigned int))

#define DMA_CMD_PACKET_L2TT2L_COPY_DWORDS                           \
    (sizeof(DMA_CMD_PACKET_L2TT2L_COPY) / sizeof(unsigned int))

#define DMA_CMD_PACKET_NOP_DWORDS                                   \
    (sizeof(DMA_CMD_PACKET_NOP) / sizeof(unsigned int))

#define DMA_CMD_PACKET_SRBM_WRITE_DWORDS                            \
    (sizeof(DMA_CMD_PACKET_SRBM_WRITE) / sizeof(unsigned int))

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  //  _SDMA10_PKT_H_
