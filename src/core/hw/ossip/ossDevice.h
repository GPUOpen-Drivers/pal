/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "pal.h"
#include "core/cmdStream.h"

namespace Pal
{

class  CmdBuffer;
struct CmdBufferCreateInfo;
class  Device;
class  Engine;
class  Queue;
class  QueueContext;
struct CmdBufferCreateInfo;
struct QueueCreateInfo;

// =====================================================================================================================
// Abstract interface for accessing a Device's hardware-specific functionality common to all OSSIP hardware layers.
class OssDevice
{
public:
    // Destroys the OssDevice object without freeing the system memory the object occupies.
    void Destroy() { this->~OssDevice(); }

    virtual Result CreateEngine(
        EngineType engineType,
        uint32     engineIndex,
        Engine**   ppEngine) = 0;

    virtual Result CreateDummyCommandStream(EngineType engineType, Pal::CmdStream** ppCmdStream) const = 0;

    // Determines the amount of storage needed for a QueueContext object for the given Queue type and ID. For Queue
    // types not supported by OSSIP hardware blocks, this should return zero.
    virtual size_t GetQueueContextSize(const QueueCreateInfo& createInfo) const = 0;

    // Constructs a new QueueContext object in preallocated memory for the specified parent Queue. This should always
    // fail with Result::ErrorUnavailable when called on a Queue which OSSIP hardware blocks don't support.
    virtual Result CreateQueueContext(
        Queue*         pQueue,
        void*          pPlacementAddr,
        QueueContext** ppQueueContext) = 0;

    // Determines the type of storage needed for a CmdBuffer.
    virtual size_t GetCmdBufferSize() const = 0;

    // Constructs a new CmdBuffer object in preallocated memory.
    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        CmdBuffer**                ppCmdBuffer) = 0;

    Device* Parent() const { return m_pParent; }

protected:
    OssDevice(Device* pDevice) : m_pParent(pDevice) {}
    virtual ~OssDevice() { }

private:
    Device*const  m_pParent;
};

// NOTE: Below are prototypes for several utility functions for each OSSIP namespace in PAL. These functions act as
// factories for creating GfxDevice objects for a specific hardware layer. Each OSSIP namespace must export the
// following functions:
//
// size_t GetDeviceSize();
// * This function returns the size in bytes needed for an OssDevice object associated with a Pal::Device object.
//
// Result CreateDevice(Device* pDevice, void* pPlacementAddr, OssDevice** ppOssDevice);
// * This function is the actual factory for creating OssDevice objects. It creates a new object in the specified
//   preallocated memory buffer and returns a pointer to that object through ppOssDevice.

#if PAL_BUILD_OSS1
namespace Oss1
{
extern size_t GetDeviceSize();
extern Result CreateDevice(
    Device*      pDevice,
    void*        pPlacementAddr,
    OssDevice**  ppOssDevice);
} // Oss1
#endif

#if PAL_BUILD_OSS2
namespace Oss2
{
extern size_t GetDeviceSize();
extern Result CreateDevice(
    Device*      pDevice,
    void*        pPlacementAddr,
    OssDevice**  ppOssDevice);
} // Oss2
#endif

#if PAL_BUILD_OSS2_4
namespace Oss2_4
{
extern size_t GetDeviceSize();
extern Result CreateDevice(
    Device*      pDevice,
    void*        pPlacementAddr,
    OssDevice**  ppOssDevice);
} // Oss2_4
#endif

#if PAL_BUILD_OSS4
namespace Oss4
{
extern size_t GetDeviceSize();
extern Result CreateDevice(
    Device*      pDevice,
    void*        pPlacementAddr,
    OssDevice**  ppOssDevice);
} // Oss4
#endif

} // Pal
