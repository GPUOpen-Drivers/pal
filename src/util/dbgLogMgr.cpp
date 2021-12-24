/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_ENABLE_LOGGING
#include "palDbgLogMgr.h"
#include "palDbgLogger.h"
#include "palIntrusiveListImpl.h"

namespace Util
{
// A global DbgLogMgr object available for use by any driver component
// from start to end of the apllication.
DbgLogMgr g_dbgLogMgr;

// Dummy variable used to set thread local pointer to point to this in
// order to distinguish between nullptr and some non-null address.
static bool g_reentryGuard = true;

// =====================================================================================================================
// Generic debug log function called by PAL_DPF* macros.
void DbgLog(
    SeverityLevel   severity,
    OriginationType source,
    const char*     pClientTag,
    const char*     pFormat,  // Printf-style format string.
    ...)                      // Printf-style argument list.
{
    va_list argList;
    va_start(argList, pFormat);
    g_dbgLogMgr.LogMessage(severity, source, pClientTag, pFormat, argList);
    va_end(argList);
}

// =====================================================================================================================
// Constructor that initializes all the data memebers with memory allocation for the pointer types.
DbgLogMgr::DbgLogMgr() :
    m_logEnabled(true),
    m_reentryGuardKey{},
    m_dbgLoggersLock(),
    m_dbgLoggersList()
{
    Result result = Util::CreateThreadLocalKey(&m_reentryGuardKey);

    // Set an internal error as the result of data member initialization results. Clients can
    // query this and decide if they want to proceed with using the DbgLogMgr having internal errors.
    m_error = (result < Result::Success);
}

// =====================================================================================================================
// Destructor that cleans up all of the allocated memory.
DbgLogMgr::~DbgLogMgr()
{
    m_logEnabled = false; // set to false since no point trying to log anything at this time.
    Util::DeleteThreadLocalKey(m_reentryGuardKey);

    // At this time the debug loggers list should be empty if each client has detached and deleted
    // the loggers it created. If not, assert to notify us of this state.
    //
    // No cleanup (unlinking the pointers) of the debug loggers list will be done here due to the following reasons:
    // 1. DbgLogMgr will not delete the debug loggers in its list since it doesn't own them
    // 2. Since DbgLogMgr is a global variable, it gets created and destroyed just when the driver is loaded
    //    and unloaded. During DbgLogMgr's destruction, it's too late to access the logger's heap memory to
    //    get to the next node of the list. The heap can become inconsistent just before DLL unload when this
    //    destructor is called.
    // Due to these reasons, let the OS handle the cleanup during DLL detach and process termination.
    PAL_ASSERT(m_dbgLoggersList.IsEmpty());
}

// =====================================================================================================================
// Insert the given debug logger to the debug loggers list.
Result DbgLogMgr::AttachDbgLogger(
    IDbgLogger* pDbgLogger)
{
    Result result = Result::ErrorUnknown;

    // Following code can generate PAL_ASSERT messages which may or may not get logged depending on the
    // debug loggers list. If list is empty, then there are no loggers to perform logging. In this case,
    // such messages will not be logged. Irrespective, the same thread will try to log messages in LogMessage()
    // and while doing so, it will try to acquire the lock that it already has acquired over here, causing a
    // deadlock. To avoid this deadlock, set the thread local guard over here so that this thread doesn't enter
    // LogMessage() at all while it is executing code from here.
    if (pDbgLogger == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    // set to a non-null address
    else if ((m_error == false) && (Util::SetThreadLocalValue(m_reentryGuardKey, &g_reentryGuard) == Result::Success))
    {
        {
            RWLockAuto<RWLock::ReadWrite> lock(&m_dbgLoggersLock);
            m_dbgLoggersList.PushBack(pDbgLogger->ListNode());
            result = Result::Success;
        }
        Util::SetThreadLocalValue(m_reentryGuardKey, nullptr);
    }

    return result;
}

// =====================================================================================================================
// Remove the given debug logger from the debug loggers list.
Result DbgLogMgr::DetachDbgLogger(
    IDbgLogger* pDbgLogger)
{
    Result result = Result::ErrorUnknown;

    // Following code can generate PAL_ASSERT messages which may or may not get logged depending on the
    // debug loggers list. If list is empty, then there are no loggers to perform logging. In this case,
    // such messages will not be logged. Irrespective, the same thread will try to log messages in LogMessage()
    // and while doing so, it will try to acquire the lock that it already has acquired over here, causing a
    // deadlock. To avoid this deadlock, set the thread local guard over here so that this thread doesn't enter
    // LogMessage() at all while it is executing code from here.
    if (pDbgLogger == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }
    // set to a non-null address
    else if ((m_error == false) && (Util::SetThreadLocalValue(m_reentryGuardKey, &g_reentryGuard) == Result::Success))
    {
        {
            RWLockAuto<RWLock::ReadWrite> lock(&m_dbgLoggersLock);
            m_dbgLoggersList.Erase(pDbgLogger->ListNode());
        }
        Util::SetThreadLocalValue(m_reentryGuardKey, nullptr);
    }

    return result;
}

// =====================================================================================================================
// Call each logger's LogMessage(). The actual logging will be done by each of these loggers.
void DbgLogMgr::LogMessage(
    SeverityLevel   severity,
    OriginationType source,
    const char*     pClientTag,
    const char*     pFormat,
    va_list         args)
{
    // The code below calls functions outside of DbgLogMgr and so it is quite
    // likey that this chain of call may end up calling LogMessage() again. This
    // can cause infinitely recursing loop. To prevent this, use a guard mechanism
    // through thread local variable that will be set when this function is called
    // for the first time and unset when it exits. With this, the thread will only
    // enter this function if it is the first time.
    //
    // This implies that log messages generated during this function's execution
    // will not get logged.
    //
    // The guard check must be the first statement in this function
    if ((m_error == false) &&
        (Util::GetThreadLocalValue(m_reentryGuardKey) == nullptr) &&
        (Util::SetThreadLocalValue(m_reentryGuardKey, &g_reentryGuard) == Result::Success)) // set to non null addr
    {
        {
            RWLockAuto<RWLock::ReadOnly> lock(&m_dbgLoggersLock);
            if (m_logEnabled)
            {
                for (auto iter = m_dbgLoggersList.Begin(); iter.IsValid(); iter.Next())
                {
                    iter.Get()->LogMessage(severity, source, pClientTag, pFormat, args);
                }
            }
        }
        Util::SetThreadLocalValue(m_reentryGuardKey, nullptr);
    }
}

// =====================================================================================================================
// Call each logger's LogMessage() passing it the incoming raw data. Each logger will know how to handle the raw
// data and log it appropriately.
void DbgLogMgr::LogMessage(
    SeverityLevel   severity,
    OriginationType source,
    const char*     clientTag,
    size_t          dataSize,
    const void*     pData)
{
    // TODO in the next phase of development
    PAL_NOT_IMPLEMENTED();
}
} // Util
#endif
