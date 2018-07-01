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
/**
 ***********************************************************************************************************************
 * @file  palEvent.h
 * @brief PAL utility collection Event class declaration.
 ***********************************************************************************************************************
 */

#pragma once

#include "palUtil.h"

namespace Util
{

/**
 ***********************************************************************************************************************
 * @brief Synchronization primitive that can either be in the _set_ or _reset_ state.
 *
 * Threads can call WaitForEvents() to block waiting for an Event object to be _set_.  This is useful for fine-grain
 * synchronization between threads.
 *
 * Event objects start out in the _reset_ state.
 ***********************************************************************************************************************
 */
class Event
{
public:
    Event();
    ~Event();

    /// Initializes the event object.  Clients must call this before using the Event object.
    ///
    /// @param manualReset          If true, the event is created as manual reset.
    /// @param initiallySignaled    If true, the event is created in signaled state.
    /// @param canBeInherited       If true, the event can be inherited by child process, it's Windows-specific.
    /// @param pName                Specified the event's name, it's Windows-specific, Windows uses this name to
    ///                             uniquely identify fence objects across processes.
    ///
    /// @returns Success if the event was successfully initialized, otherwise an appropriate error code.
    Result Init(
        bool manualReset        = true,
        bool initiallySignaled  = false
        );

    /// Changes the event state to _set_
    ///
    /// @returns Success unless the Event has not been initialized yet (@ref ErrorUnavailable) or an unexpected internal
    ///          error occured when calling the OS (ErrorUnknown).
    Result Set() const;

    /// Changes the event state to _reset_.
    ///
    /// @returns Success unless the Event has not been initialized yet (ErrorUnavailable) or an unexpected
    ///         internal error occured when calling the OS (ErrorUnknown).
    Result Reset() const;

    /// Waits for the event to enter the _set_ state before returning control to the caller.  The event will change to
    /// the _reset_ state if manualReset was false on initialization.
    ///
    /// @param [in] timeout Max time to wait, in seconds.  If zero, this call will poll the event without blocking.
    ///
    /// @returns Success if the wait completed successfully or Timeout if the wait did not complete but the operation
    ///          timed out.  Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidValue will be returned if the timeout is negative.
    ///          + ErrorUnknown may be returned if an unexpected internal occurs when calling the OS.
    Result Wait(float timeout) const;

    /// On Linux, a handle to an OS event primitive is a file descriptor, which is just an int.
    typedef int32 EventHandle;

    /// Returns a handle to the actual OS event primitive associated with this object.
    EventHandle GetHandle() const { return m_hEvent; }

    /// Open event handle.
    Result Open(EventHandle handle, bool isReference);

    /// Constant EventHandle value which represents an invalid event object.
    static const EventHandle InvalidEvent;

private:
    EventHandle m_hEvent;      // OS-specific event handle.
    bool        m_isReference; // If true, the event is a global sharing object handle (not a duplicate) which is
                               // imported from external, so it can't be closed in the currect destructor, and can only
                               // be closed by the creater.

    PAL_DISALLOW_COPY_AND_ASSIGN(Event);
};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 412
/// Waits for one or more events to enter the _set_ state before returning control to the caller.
///
/// @param [in] pAllocator If this function needs to allocate memory, it uses the specified allocator.
/// @param [in] ppEvents   Array of one or more Event objects to wait on.
/// @param [in] eventCount Number of events to wait on (size of the ppEvents array).
/// @param [in] waitAll    True if this function should block until _all_ specified events have been signaled.  False
///                        if this function should unblock when _any_ event in the list is signaled.
/// @param [in] timeout    Max time to wait, in seconds.  Must be >= 0.
///
/// @returns Success if the wait completed successfully or Timeout if the wait did not complete but the operation timed
///          out.  Otherwise, one of the following errors may be returned:
///          + ErrorUnknown may be returned if an unexpected internal occurs when calling the OS.
///          + ErrorOutOfMemory may be returned if an internal memory allocation failed.
template <typename Allocator>
extern Result WaitForEvents(
    Allocator*         pAllocator,
    const Event*const* ppEvents,
    uint32             eventCount,
    bool               waitAll,
    float              timeout);
#endif

} // Util
