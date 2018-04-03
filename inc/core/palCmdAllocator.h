/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palCmdAllocator.h
 * @brief Defines the Platform Abstraction Library (PAL) ICmdAllocator interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palDestroyable.h"

namespace Pal
{

// Forward declarations.
class IGpuMemory;

/// Flags controlling the creation of ICmdAllocator objects.
union CmdAllocatorCreateFlags
{
    struct
    {
        uint32 threadSafe               :  1; ///< If set, the allocator will acquire a lock each time it is accessed;
                                              ///  otherwise it will not attempt to protect itself from multithreaded
                                              ///  access.
        uint32 autoMemoryReuse          :  1; ///< If set, the allocator will track when the GPU finishes accessing
                                              ///  each piece of command memory and attempt to reuse memory which the
                                              ///  GPU is done with before allocating more memory from the OS.  If not
                                              ///  set, memory will only be recycled after a call to
                                              ///  @ref ICmdAllocator::Reset().
        uint32 disableBusyChunkTracking :  1; ///< If set, the allocator will not do any GPU-side tracking of which
                                              ///  command chunks are still in use.  It will be the client's (or the
                                              ///  application's) responsibility to guarantee that command chunks are
                                              ///  not returned to the allocator before the GPU has finished processing
                                              ///  them.  Failure to guarantee this will result in undefined behavior.
                                              ///  This flag has no effect if @ref autoMemoryReuse is not set.
        uint32 reserved                 : 29; ///< Reserved for future use.
    };

    uint32     u32All;          ///< Flags packed as 32-bit uint.
};

/// Different type of allocation data that an ICmdAllocator allocates and distributes to command buffers.
enum CmdAllocType : uint32
{
    CommandDataAlloc  = 0,  ///< Data allocated is for executable commands.
    EmbeddedDataAlloc,      ///< Data allocated is for embedded data.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 395
    GpuScratchMemAlloc,     ///< Data allocated is GPU-only accessible at command buffer execution-time.  Possible
                            ///  uses include CE RAM dumps and GPU events.
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 395
    GpuScratchMemAlloc,     ///< Data allocated is GPU-only accessible at command buffer execution-time.  Possible
                            ///  uses include CE RAM dumps and GPU events.
#endif
    CmdAllocatorTypeCount   ///< Number of allocation types for ICmdAllocator's.
};

/// Specifies properties for creation of an ICmdAllocator object.  Input structure to IDevice::CreateCmdAllocator().
struct CmdAllocatorCreateInfo
{
    CmdAllocatorCreateFlags flags;        ///< Flags controlling command allocator creation.

    struct
    {
        GpuHeap             allocHeap;    ///< Preferred allocation heap.  For @ref GpuScratchMemAlloc, this field is
                                          ///  ignored and the allocation will always be in GPU-invisible memory.  For
                                          ///  all other allocation types, this must be CPU-mappable.
                                          ///  For best performance, command allocators that will be used by the
                                          ///  UVD engine should prefer the Local heap
        gpusize             allocSize;    ///< Size, in bytes, of the GPU memory allocations this allocator will create.
                                          ///  It must be an integer multiple of suballocSize.
        gpusize             suballocSize; ///< Size, in bytes, of the chunks of GPU memory this allocator will give to
                                          ///  command buffers.  It must be an integer multiple of 4096.
                                          ///  Must be greater than zero even if the client doesn't plan on using this
                                          ///  allocation type.
    } allocInfo[CmdAllocatorTypeCount];   ///< Information for each allocation type.
};

/**
 ***********************************************************************************************************************
 * @interface ICmdAllocator
 * @brief     Allocates and distributes GPU memory to command buffers on the client's behalf.
 *
 * All ICmdBuffer objects must be associated with an ICmdAllocator at creation. Command buffers may switch command
 * allocators when ICmdBuffer::Reset() is called. The set of command buffers associated with a given command allocator
 * will query that allocator for additional GPU memory as they are building commands.
 *
 * To protect against race conditions the client must ask for a thread safe command allocator unless its can guarantee
 * that all command buffers associated with a given command allocator will be built, reset, and destroyed in a thread-
 * safe manner. It is illegal to destroy a command allocator while it still has command buffers associated with it.
 *
 * @see IDevice::CreateCmdAllocator()
 ***********************************************************************************************************************
 */
class ICmdAllocator : public IDestroyable
{
public:
    /// Explicitly resets a command allocator, marking all internal GPU memory allocations as unused.
    ///
    /// The client is responsible for guaranteeing that all command buffers associated with this allocator have finished
    /// GPU execution and have been explicitly reset before calling this function.
    ///
    /// @returns Success if the command allocator was successfully reset.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorUnknown if an internal PAL error occurs.
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
    ICmdAllocator() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~ICmdAllocator() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
