/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "core/layers/decorators.h"
#include "core/layers/interfaceLogger/interfaceLoggerLogContext.h"
#include "palDevice.h"
#include "palMutex.h"
#include "palThread.h"
#include "palVector.h"

#if defined(__unix__)
#include <time.h>
#endif

namespace Pal
{
namespace InterfaceLogger
{

// Abstract the OS-dependent timer types for internal use.
#if   defined(__unix__)
typedef timespec RawTimerVal;
#endif

enum InterfaceLogFlags : uint32
{
    LogFlagGeneralCalls  = 0x00000001,
    LogFlagCreateDestroy = 0x00000002,
    LogFlagBindGpuMemory = 0x00000004,
    LogFlagQueueOps      = 0x00000008,
    LogFlagCmdBuilding   = 0x00000010,
    LogFlagCreateSrds    = 0x00000020,
    LogFlagCallbacks     = 0x00000040,
    LogFlagBarrierLog    = 0x00000080,
    LogFlagBarrierLogCr  = 0x80000000 // Internal only flag
};

// =====================================================================================================================
// Some data we will track for each thread.
class ThreadData
{
    typedef Util::Vector<DevCallbackArgs, 16, Platform> CallbackVector;

public:
    ThreadData(Platform* pPlatform, uint32 threadId);
    ~ThreadData();

    void SetContext(LogContext* pContext);
    void StartCall(uint32 objectId, InterfaceFunc func, uint64 preCallTime);
    void EndCall();

    void PushBackCallbackArgs(
        Developer::CallbackType callbackType,
        const void*             pCallbackData,
        size_t                  dataSize);
    void ClearCallbackArgs();

    // StartCall and EndCall use InterfaceFunc::Count as a sentinel value which means "logging disabled".
    bool LoggingActive() const { return m_activeFunc != InterfaceFunc::Count; }
    bool HasCallbacks()  const { return m_callbacks.IsEmpty() == false; }

    LogContext*           Context()     const { return m_pContext; }
    uint32                ThreadId()    const { return m_threadId; }
    uint32                ObjectId()    const { return m_objectId; }
    InterfaceFunc         ActiveFunc()  const { return m_activeFunc; }
    uint64                PreCallTime() const { return m_preCallTime; }
    const CallbackVector& Callbacks()   const { return m_callbacks; }

private:
    LogContext*    m_pContext;    // Non-null only if we're in mutithreaded logging mode.
    const uint32   m_threadId;    // A monotonic ID assigned by the Platform, not a real OS thread ID.

    // This state describes the interface function call that this thread is currently executing. These values will
    // change each time this thread calls StartCall and EndCall (see the Platform's ActivateLogging function).
    uint32         m_objectId;    // The PAL interface object being called.
    InterfaceFunc  m_activeFunc;  // The function this thread is actively logging or InterfaceFunc::Count.
    uint64         m_preCallTime; // The tick immediately before calling down to the next layer.
    CallbackVector m_callbacks;   // Developer callbacks that occured during this interface function call.
};

// =====================================================================================================================
class Platform final : public PlatformDecorator
{
    // All ThreadData instances will be stored in a vector so we can delete them later.
    typedef Util::Vector<ThreadData*, 16, Platform> ThreadDataVector;

public:
    static Result Create(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb,
        IPlatform*                  pNextPlatform,
        bool                        enabled,
        void*                       pPlacementAddr,
        IPlatform**                 ppPlatform);

    Platform(
        const PlatformCreateInfo&   createInfo,
        const Util::AllocCallbacks& allocCb,
        IPlatform*                  pNextPlatform,
        bool                        enabled);
    virtual ~Platform();

    // Must be called when a device has its settings committed so that we can determine exactly what logging modes are
    // enabled. Prior to calling this, the platform will record data in the main log but it won't be flushed to a file.
    Result CommitLoggingSettings();

    // Must be called by other InterfaceLogger classes whenever a new frame is presented. Cannot be called between
    // LogBeginFunc and LogEndFunc as this may deadlock single-threaded logging.
    void UpdatePresentState();

    // Returns the current clock time in ticks relative to the starting time.
    uint64 GetTime() const;

    // Interface function calls must be logged using this process:
    // 1. Call ActivateLogging, it will return true if this call should be logged.
    // 2. Call the next layer's version of this interface function. (Except for Destroy functions!)
    // 3. If ActivateLogging returned true:
    //     a. Call LogBeginFunc. It is guaranteed to return a non-null LogContext pointer.
    //     b. Log this function's inputs and outputs, if any.
    //     c. Call LogEndFunc.
    // If this process is not followed things may break. The log file might be invalid, a thread might deadlock, or
    // the process might simply crash.
    bool        ActivateLogging(uint32 objectId, InterfaceFunc func);
    LogContext* LogBeginFunc();
    void        LogEndFunc(LogContext* pContext);

    // Returns a new object ID for an object of the given type. Note that AtomicIncrement returns the result of the
    // increment so we must subtract one to get the ID for the current object.
    uint32 NewObjectId(InterfaceObject objectType)
        { return Util::AtomicIncrement(m_nextObjectIds + static_cast<uint32>(objectType)) - 1; }

    // Public IPlatform interface methods:
    virtual Result EnumerateDevices(uint32* pDeviceCount, IDevice* pDevices[MaxDevices]) override;
    virtual size_t GetScreenObjectSize() const override;
    virtual Result GetScreens(
        uint32*  pScreenCount,
        void*    pStorage[MaxScreens],
        IScreen* pScreens[MaxScreens]) override;

    virtual Result TurboSyncControl(
        const TurboSyncControlInput& turboSyncControlInput) override;

    // Public IDestroyable interface methods:
    virtual void Destroy() override;

    static void PAL_STDCALL InterfaceLoggerCb(
        void*                   pPrivateData,
        const uint32            deviceIndex,
        Developer::CallbackType type,
        void*                   pCbData);

    uint32 FrameCount() const { return m_frameCount; }

    bool IsBarrierLogActive() const;

protected:
    virtual Result Init() override;

private:
    ThreadData* CreateThreadData();
    LogContext* CreateThreadLogContext(uint32 threadId);

    bool IsFrameRangeActive() const;

    union
    {
        struct
        {
            uint32 threadKeyCreated  :  1; // If m_threadKey was successfully created.
            uint32 multithreaded     :  1; // If multithreaded logging is enabled.
            uint32 settingsCommitted :  1; // If the platform has all of the settings needed to log to a file.
            uint32 reserved          : 29;
        };
        uint32     u32All;
    } m_flags;

    const PlatformCreateInfo m_createInfo;        // The client's original create info.
    Util::Mutex              m_platformMutex;     // Used to serialize access various state within the platform.
    RawTimerVal              m_startTime;         // The timer value at the time the platform was initialized.
    LogContext*              m_pMainLog;          // Holds all logged data if multithreaded logging is disabled.
                                                  // Otherwise it holds some initial logged data and identifies all
                                                  // thread log files.
    uint32                   m_nextThreadId;      // Each thread file gets a unique ID (not the OS thread ID).
    uint32                   m_objectId;          // This object's unique ID.
    volatile uint32          m_activePreset;      // The index of the active preset in m_loggingPresets.
    uint32                   m_loggingPresets[2]; // Masks of logging levels that the user can select for logging.
    Util::ThreadLocalKey     m_threadKey;         // Used to look up thread specific data (e.g., thread logs).
    ThreadDataVector         m_threadDataVec;     // A list of all thread-local data so they can be deleted on exit.
    uint32                   m_frameCount;        // Number of frames presented.

    // Tracks the next ID to be issued for all objects.
    volatile uint32          m_nextObjectIds[static_cast<uint32>(InterfaceObject::Count)];

    PAL_DISALLOW_DEFAULT_CTOR(Platform);
    PAL_DISALLOW_COPY_AND_ASSIGN(Platform);
};

} // InterfaceLogger
} // Pal

#endif
