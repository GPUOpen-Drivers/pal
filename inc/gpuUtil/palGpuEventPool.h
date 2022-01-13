/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

class ICmdBuffer;
class IGpuEvent;
}

namespace GpuUtil
{

/**
***********************************************************************************************************************
* @class GpuEventPool
* @brief Helper class providing a pool to efficiently manage IGPuEvent objects.
*
* A GpuEventPool is a container for a set of GPU event objects. Its main purpose is to provide client with a utility to
* efficiently manage PAL's GPU events.
*
* @warning GpuEventPool is not thread safe.  Acquire event or recycle event from different threads should use
*          different pool objects.
***********************************************************************************************************************
*/
template <typename PlatformAllocator, typename GpuEventAllocator>
class GpuEventPool
{
    typedef Util::Deque<Pal::IGpuEvent*, PlatformAllocator> GpuEventDeque;

public:
    /// Constructor.
    ///
    /// @param [in] pDevice             The device this pool is based on.
    /// @param [in] pPlatformAllocator  The allocator that allocates class owned list objects.
    /// @param [in] pAllocator          The allocator that allocates GPU event objects.
    GpuEventPool(
        Pal::IDevice*      pDevice,
        PlatformAllocator* pPlatformAllocator,
        GpuEventAllocator* pAllocator);

    /// Destructor.
    ///
    /// Cleans up all allocated GPU event objects in this pool.
    ~GpuEventPool();

    /// Reset the pool by releasing all GPU events (both the backing system memory and video memory) back to allocator.
    /// This should only be called after all work referring to those events have finished on GPU.
    Pal::Result Reset();

    /// Provide an available GPU event from free event list, or allocate a new one if the list is empty.
    /// A newly created GPU event gets a new allocated GPU memory, the backing video mem is GPU-access only scratch
    /// memory from the invisible heap.
    ///
    /// @param [in]  pCmdBuffer  Provides an allocator and a way to allocate video memory for GPU event.
    /// @param [out] ppEvent     The provided available event.
    Pal::Result GetFreeEvent(Pal::ICmdBuffer* pCmdBuffer, Pal::IGpuEvent**const ppEvent);

    /// Return a GPU event back to free event list. The returned GPU event is regarded freed and can be reused at any
    /// time without the need to re-allocating video memory or initilization. The event value is not guaranteed so
    /// client needs to reset the value before use. Need to reset from GPU because the video memory is GPU-access only.
    Pal::Result ReturnEvent(Pal::IGpuEvent* pEvent);

private:
    // Create a new GPU event object and allocate video memory for it. A GPU-access only scratch memory from the
    // invisible heap is allocated.
    Pal::Result CreateNewEvent(Pal::ICmdBuffer* pCmdBuffer, Pal::IGpuEvent**const ppEvent);

    Pal::IDevice*const      m_pDevice;
    GpuEventAllocator*const m_pAllocator; // System memory allocator that allocates GPU event objects

    GpuEventDeque m_freeEventList;
    GpuEventDeque m_globalEventList;

    PAL_DISALLOW_DEFAULT_CTOR(GpuEventPool);
    PAL_DISALLOW_COPY_AND_ASSIGN(GpuEventPool);
};

} // GpuUtil
