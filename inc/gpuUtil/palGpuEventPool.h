/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  palGpuEventPool.h
* @brief PAL GPU utility GpuEventPool class.
***********************************************************************************************************************
*/

#pragma once

#include "palDeque.h"
#include "palPlatform.h"

// Forward declarations.
namespace Pal
{
class IGpuEvent;
}

namespace GpuUtil
{

/**
***********************************************************************************************************************
* @class GpuEventPool
* @brief Helper class providing a pool to efficiently manage IGPuEvent objects.
*
* A GpuEventPool is a container for a set of GpuEvent objects. Its main purpose is to provide client with a utility to
* efficiently manage PAL's GpuEvents.
*
* @warning GpuEventPool is not thread safe.  Acquire event or recycle event from different threads should use
*          different pool objects.
***********************************************************************************************************************
*/
class GpuEventPool
{
    typedef Util::Deque<Pal::IGpuEvent*, Pal::IPlatform> GpuEventDeque;

public:
    /// Constructor.
    ///
    /// @param [in] pPlatform  The allocator that will allocate memory if required.
    /// @param [in] pDevice    The device this pool is based on.
    GpuEventPool(
        Pal::IPlatform* pPlatform,
        Pal::IDevice*   pDevice);

    /// Destructor.
    ///
    /// Cleans up all allocated GpuEvent objects in this pool.
    ~GpuEventPool();

    /// Initialize the newly constructed pool by pre-allocating client-specified number of GpuEvent objects.
    ///
    /// @param [in] defaultCapacity  The default number of gpu events pre-allocated in the pool for efficiency.
    Pal::Result Init(Pal::uint32 defaultCapacity);

    /// Reset the pool by reseting and moving all allocated GpuEvent objects back to available list.
    /// This should only be called after all work referring to those events have finished
    Pal::Result Reset();

    /// Provide an unused GpuEvent from available list, or allocate a new one if available list is empty.
    ///
    /// @param [in] ppEvent  The provided available event.
    Pal::Result AcquireEvent(Pal::IGpuEvent**const ppEvent);

private:
    Pal::IPlatform*const m_pPlatform;
    Pal::IDevice*const   m_pDevice;

    GpuEventDeque m_availableEvents;
    GpuEventDeque m_busyEvents;

    PAL_DISALLOW_DEFAULT_CTOR(GpuEventPool);
    PAL_DISALLOW_COPY_AND_ASSIGN(GpuEventPool);
};

} // GpuUtil
