/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palEvent.h
 * @brief Defines the Platform Abstraction Library (PAL) IGpuEvent interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 474
#include "palGpuMemoryBindable.h"
#else
#include "palDestroyable.h"
#endif

namespace Pal
{

/// Specifies properties for creation of an IGpuEvent object.  Input structure to IDevice::CreateGpuEvent().
struct GpuEventCreateInfo
{
    union
    {
        struct
        {
            uint32 gpuAccessOnly :  1; ///< If true, GetStatus(), Set(), and Reset() must never be called.
            uint32 reserved      : 31; ///< Reserved for future use.
        };
        uint32 u32All;                 ///< Flags packed as 32-bit uint.
    } flags;                           ///< GPU event property flags.
};

/**
 ***********************************************************************************************************************
 * @interface IGpuEvent
 * @brief     Represents a GPU event object that can be used for finer-grain CPU/GPU and GPU/GPU synchronization than
 *            is available with IFence and IQueueSemaphore objects.
 *
 * An event object can be set or reset by both the CPU and GPU, and its status can be queried by the CPU.  This allows
 * the client to monitor progress of GPU execution within a command buffer.
 *
 * If the client knows that they will never examine an event object using the CPU they should set the gpuAccessOnly
 * flag but must take care to never call GetStatus(), Set(), or Reset().
 *
 * On creation, GPU events are in the "reset" state unless they are created with the gpuAccessOnly flag.  In that case
 * the client is responsible for placing the GPU event in a known state on first use (either "set" or "reset").
 *
 * @see IDevice::CreateEvent()
 ***********************************************************************************************************************
 */

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 474
class IGpuEvent: public IGpuMemoryBindable
#else
class IGpuEvent : public IDestroyable
#endif
{
public:
    /// Gets the status (set or reset) of the event.
    ///
    /// @returns EventSet if the event is currently set, or EventReset if the event is currently reset.  Other return
    ///          codes indicate an error.
    virtual Result GetStatus() = 0;

    /// Puts the event into the "set" state from the CPU.
    ///
    /// @returns Success if the event was successfully set.  Other return codes indicate an error.
    virtual Result Set() = 0;

    /// Puts the event into the "reset" state from the CPU.
    ///
    /// @returns Success if the event was successfully reset.  Other return codes indicate an error.
    virtual Result Reset() = 0;

    /// Returns the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @returns Pointer to client data.
    PAL_INLINE void* GetClientData() const
    {
        return m_pClientData;
    }

    /// Sets the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @param  [in]    pClientData     A pointer to arbitrary client data.
    PAL_INLINE void SetClientData(
        void* pClientData)
    {
        m_pClientData = pClientData;
    }

protected:
    /// @internal Constructor. Prevent use of new operator on this interface. Client must create objects by explicitly
    /// called the proper create method.
    IGpuEvent() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IGpuEvent() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
