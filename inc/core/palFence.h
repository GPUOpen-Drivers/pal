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
 * @file  palFence.h
 * @brief Defines the Platform Abstraction Library (PAL) IFence interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palDestroyable.h"

namespace Pal
{

/// Specifies properties for fence @ref IFence fence creation. Input structure to IDevice::CreateFence().
struct FenceCreateInfo
{
    union
    {
        struct
        {
            uint32 signaled            : 1;  ///< Specify whether the initial status of the fence is signaled or not.
            uint32 eventCanBeInherited : 1;  ///< The event handle can be inherited by child process.
            uint32 reserved            : 30; ///< Reserved for future use.
        };
        uint32 u32All;      ///< Flags packed as 32-bit uint.
    } flags;                ///< Fence creation flags.
};

/// Specifies properties for fence opening. Input structure to IDevice::OpenFence().
struct FenceOpenInfo
{
    union
    {
        struct
        {
            uint32 isReference  : 1;    ///< If ture, duplicate the event handle, otherwise reference the event handle.
            uint32 reserved     : 31;   ///< Reserved for future use.
        };
        uint32 u32All;                  ///< Flags packed as 32-bit uint.
    } flags;

    OsExternalHandle externalFence;     ///< External shared fence handle.
};

/**
 ***********************************************************************************************************************
 * @interface IFence
 * @brief     Represents a command buffer fence the client can use for coarse-level synchronization between the GPU and
 *            CPU.
 *
 * Fences can be specified when calling IQueue::Submit() and will be signaled when certain prior queue operations have
 * completed.  The status of the fence can be queried by the client to determine when the GPU work of interest has
 * completed.
 *
 * Fences are guaranteed to wait for:
 * + Prior command buffer submissions.
 * + Prior queue semaphore signals and waits.
 * + Prior direct presents.
 *
 * @see IDevice::CreateFence()
 ***********************************************************************************************************************
 */
class IFence : public IDestroyable
{
public:
    /// Gets the status (completed or not) of the fence.
    ///
    /// @returns Success if the fence has been reached, or NotReady if the fence hasn't been reached.  Other return
    ///          codes indicate an error:
    ///          + ErrorFenceNeverSubmitted if the fence hasn't been submitted yet and the fence is not created with
    ///            initialSignaled set to true.
    virtual Result GetStatus() const = 0;

    /// Get the event handle of the fence.
    /// @returns the handle in the type OsExternalHandle
    virtual OsExternalHandle GetHandle() const = 0;

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
    IFence() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IFence() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
