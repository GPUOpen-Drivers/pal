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

#include "palQueueSemaphore.h"
#include "core/os/lnx/lnxHeaders.h"

namespace Pal
{

class Device;
class GpuMemory;
class Queue;

// =====================================================================================================================
// Semaphore object used to synchronize GPU work performed by multiple, parallel queues.  These semaphores are used by
// calling IQueue::SignalQueueSemaphore() and IQueue::WaitQueueSemaphore().
//
// NOTE: The Windows and Linux flavors of the OS-specific portions of this object are so similar, that it seemed
// excessive to create multiple child classes just to choose between having a GPU memory object for the Linux semaphore
// or a D3DKMT_HANDLE for the Windows semaphore. The OS-specific versions are one class, controlled by conditional
// compilation, while the master/slave behavior for shared MGPU semaphores is controlled by the child classes
// MasterQueueSemaphore and OpenedQueueSemaphore.
class QueueSemaphore : public IQueueSemaphore
{
public:
    virtual ~QueueSemaphore();

    virtual Result Open(const QueueSemaphoreOpenInfo& openInfo);
    virtual Result OpenExternal(const ExternalQueueSemaphoreOpenInfo& openInfo);

    virtual OsExternalHandle ExportExternalHandle() const;

    static Result ValidateInit(const Device* pDevice, const QueueSemaphoreCreateInfo& createInfo);
    static Result ValidateOpen(const Device* pDevice, const QueueSemaphoreOpenInfo& openInfo);

    // NOTE: Part of the public IDestroyable interface.
    virtual void Destroy() override;

    virtual Result Signal(Queue* pQueue) = 0;
    virtual Result Wait(
        Queue*         pQueue,
        volatile bool* pIsStalled) = 0;

    bool IsShareable() const { return m_flags.shareable; }
    bool IsShared() const { return m_flags.shared; }
    bool IsExternalOpened() const { return m_flags.externalOpened; }

protected:
    explicit QueueSemaphore(Device* pDevice);

    virtual Result OsInit(const QueueSemaphoreCreateInfo& createInfo);
    virtual Result OsSignal(Queue* pQueue);
    virtual Result OsWait(Queue* pQueue);

    Device*const  m_pDevice;

    amdgpu_semaphore_handle m_hSemaphore;
    bool                    m_skipNextWait; // Currently amdgpu lacks a way to signal a semaphore at creation.
                                            // As a workaround, we skip the OS wait if this is set.
private:
    Result ValidateOpenExternal(const ExternalQueueSemaphoreOpenInfo& openInfo);

    union
    {
        struct
        {
            uint32 shareable         :  1; // Semaphore can be shared across APIs or processes
            uint32 shared            :  1; // Semaphore was opened from another GPU's semaphore or external handle
            uint32 externalOpened    :  1; // Semaphore was created by other APIs
            uint32 reserved          : 29;
        };
        uint32 u32All;
    } m_flags;

    PAL_DISALLOW_DEFAULT_CTOR(QueueSemaphore);
    PAL_DISALLOW_COPY_AND_ASSIGN(QueueSemaphore);
};

} // Pal
