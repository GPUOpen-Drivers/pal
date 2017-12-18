/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
 ***********************************************************************************************************************
 * @file  palQueueSemaphore.h
 * @brief Defines the Platform Abstraction Library (PAL) IQueueSemaphore interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palDestroyable.h"

namespace Pal
{

// Forward declarations.
class IQueueSemaphore;

/// Specifies properties for @ref IQueueSemaphore creation.  Input structure to IDevice::CreateQueueSemaphore().
struct QueueSemaphoreCreateInfo
{
    union
    {
        struct
        {
            uint32 shareable         :  1;  ///< This queue semaphore may be opened for use by a different device.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 290
            uint32 sharedViaNtHandle :  1;  ///< This queue semaphore can only be shared through Nt handle.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 350
            uint32 externalOpened    :  1;  ///< Semaphore was created by other APIs
            uint32 reserved          : 29;  ///< Reserved for future use.
#else
            uint32 reserved          : 30;  ///< Reserved for future use.
#endif

#else
            uint32 reserved          : 31;
#endif
        };
        uint32 u32All;              ///< Flags packed as 32-bit uint.
    } flags;                        ///< Queue semaphore creation flags.

    uint32 maxCount;                ///< The maximum signal count; once reached, further signals are dropped.  Must be
                                    ///  non-zero and no more than maxSemaphoreCount in @ref DeviceProperties.  For
                                    ///  example, a value of one would request a binary semaphore.
    uint32 initialCount;            ///< Initial count value for the semaphore.  Must not be larger than maxCount.
};

/// Specifies parameters for opening a queue semaphore for use on another device.  Input structure to
/// IDevice::OpenSharedQueueSemaphore().
struct QueueSemaphoreOpenInfo
{
    /// Shared queue semaphore object from another device to be opened.
    IQueueSemaphore* pSharedQueueSemaphore;
};

/// Specifies parameters for opening a queue semaphore created by other APIs such as D3D.
struct ExternalQueueSemaphoreOpenInfo
{
    union
    {
        struct
        {
            uint32 crossProcess       :  1;   ///< This semaphore is created in another process.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 290
            uint32 sharedViaNtHandle  :  1;   ///< The shared semaphore handle is NT handle.
            uint32 reserved           : 30;   ///< Resevered for future use.
#else
            uint32 reserved           : 31;
#endif
        };
        uint32 u32All;                  ///< Flags packed as 32-bit uint.
    } flags;                            ///< External queue semaphore open flags.

    OsExternalHandle externalSemaphore; ///< External shared semaphore handle.
};

/**
 ***********************************************************************************************************************
 * @interface IQueueSemaphore
 * @brief     Semaphore object used to synchronize GPU work performed by multiple, parallel queues.
 *
 * These semaphores are used by calling IQueue::SignalQueueSemaphore() and IQueue::WaitQueueSemaphore().
 *
 * @see IDevice::CreateQueueSemaphore()
 * @see IDevice::OpenSharedQueueSemaphore()
 ***********************************************************************************************************************
 */
class IQueueSemaphore : public IDestroyable
{
public:
    /// An IQueue::WaitQueueSemaphore operation may need to be sent down to the OS after the corresponding
    /// IQueue::SignalQueueSemaphore operation due to GPU scheduler limitations. This method checks if any queues have
    /// batched-up commands waiting for a SignalQueueSemaphore operation to appear.
    ///
    /// @returns True if one or more queues have some number of commands batched-up waiting for other queues to signal
    ///          this semaphore. False otherwise.
    virtual bool HasStalledQueues() = 0;

    /// Returns an OS-specific handle which can be used to refer to this semaphore object across processes. This will
    /// return a null or invalid handle if the object was not created with the external create flag set.
    ///
    /// @note This function is only available for Linux builds.
    ///
    /// @returns An OS-specific handle which can be used to access the semaphore object across processes.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 290
    virtual OsExternalHandle ExportExternalHandle() const = 0;
#else
    virtual OsExternalHandle ExportExternalHandle() const = 0;
#endif

    /// Returns an OS-specific handle which can be used by another device to access the semaphore object.
    /// This interface is used to share semaphore between mantle and d3d in same process.
    ///
    /// @returns An OS-specific handle which can be used by another device to access the semaphore object.
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
    IQueueSemaphore() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IQueueSemaphore() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
