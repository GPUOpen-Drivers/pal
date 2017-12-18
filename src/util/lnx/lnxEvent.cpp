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

#include "palAutoBuffer.h"
#include "palEvent.h"

#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <sys/timerfd.h>

namespace Util
{

// Represents an invalid event handle (file descriptor) on Linux platforms.
const Event::EventHandle Event::InvalidEvent = -1;

// =====================================================================================================================
Event::Event()
    :
    m_hEvent(InvalidEvent),
    m_isReference(false)
{
}

// =====================================================================================================================
Event::~Event()
{
    if (m_hEvent != InvalidEvent)
    {
        const int32 result = close(m_hEvent);
        PAL_ALERT(result == -1);

        m_hEvent = InvalidEvent;
    }
}

// =====================================================================================================================
// Used to initialize a static event object, not needed (although not dangerous) for dynamic event objects.
//
// On Linux, we're using "eventfd" objects to represent the manual-reset event used on Windows platforms.  An eventfd is
// a file descriptor which can be used as an wait/notify mechanism by userspace applications and by the kernel to notify
// userspace applications of events.  This mechanism was chosen because it is the most likely candidate for the kernel
// graphics driver to be able to notify the UMD of event ocurrences.
//
// SEE: http://man7.org/linux/man-pages/man2/eventfd.2.html
Result Event::Init(
    bool manualReset,
    bool initiallySignaled)
{
    Result result = Result::Success;
    PAL_ASSERT(manualReset == true);
    // Create a new eventfd object with the following properties:
    //   - Non-blocking;
    //   - Non-semaphore;
    const uint32 initialState = initiallySignaled ? 1 : 0;

    m_hEvent = eventfd(initialState, EFD_NONBLOCK);

    if (m_hEvent == InvalidEvent)
    {
        PAL_ALERT_ALWAYS();
        result = Result::ErrorInitializationFailed;
    }

    return result;
}

// =====================================================================================================================
// Sets the Event object (i.e., puts it into a signaled state).
Result Event::Set() const
{
    Result result = Result::ErrorUnavailable;

    // According to the Linux man pages for eventfd, writing data to a non-blocking, non-semaphore eventfd will add the
    // data contained in the supplied buffer to the eventfd object's current counter.  It is invalid to add a negative
    // number to the eventfd object's counter using this function.  If the write will cause the event counter to
    // overflow, nothing happens and -1 is returned (errno is set to EAGAIN).

    if (m_hEvent != InvalidEvent)
    {
        const uint64 incrementValue = 1;
        if (write(m_hEvent, &incrementValue, sizeof(incrementValue)) < 0)
        {
            // EAGAIN indicates that the event's counter would have overflowed. This should never happen with us adding
            // 1 each time, because we'd need 2^64-1 calls to Set() between calls to Reset().
            PAL_ASSERT(errno == EAGAIN);
        }

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Resets the Event object (i.e., puts it into a non-signaled state).
Result Event::Reset() const
{
    Result result = Result::ErrorUnavailable;

    // According to the Linux man pages for eventfd, reading data from a non-blocking, non-semaphore eventfd will cause
    // the eventfd object's current counter to be copied to the output buffer and the counter to be reset to zero if the
    // counter has a nonzero value at the time the read is attempted. If the read is attempted when the event is already
    // in the non-signaled state, nothing happens and -1 is returned (errno is set to EAGAIN).

    if (m_hEvent != InvalidEvent)
    {
        uint64 previousValue = 0;
        if (read(m_hEvent, &previousValue, sizeof(previousValue)) < 0)
        {
            // EAGAIN indicates the event was already in the non-signaled state.
            PAL_ASSERT(errno == EAGAIN);
        }

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
Result Event::Open(
    EventHandle handle,
    bool        isReference)
{
    return Result::Unsupported;
}

// =====================================================================================================================
// Waits for one (or all, if waitAll == true) of the supplied events to become signaled.
template <typename Allocator>
Result WaitForEvents(
    Allocator*         pAllocator,  // [in] Allocator for anything we might need to allocate.
    const Event*const* ppEvents,    // [in] Events that we're going to wait for.
    uint32             eventCount,  // Number of events pointed to by ppEvents.
    bool               waitAll,     // True if this func should stall until all events complete.
    float              timeout)     // Max time to wait, in seconds.
{
    PAL_ASSERT(timeout >= 0.0f);

    // Conversion factor to go from seconds to nanoseconds.
    static constexpr float ToNanoseconds = (10E+9);

    timespec spec = { };
    spec.tv_sec   = static_cast<int32>(timeout);
    spec.tv_nsec  = static_cast<int32>((timeout - static_cast<float>(spec.tv_sec)) * ToNanoseconds);

    Result result = Result::Success;

    // According to the Linux man pages for eventfd, any of the select(), poll(), or epoll() APIs will treat the eventfd
    // object as "readable" or "ready to be read from" when the object is in the signaled state. We can therefore use
    // any of those APIs to multiplex the set of events we need to wait on.

    if ((eventCount == 1) || (waitAll == false))
    {
        // The simple case is when we only care about one of the supplied events to become signaled.  This occurs when
        // either waitAll is not requested or there's only a single event to worry about.

        // Assemble an fd_set structure to pass to the pselect() system call:
        fd_set eventSet;
        FD_ZERO(&eventSet);

        int32 maxEventFd = 0;
        for (uint32 idx = 0; idx < eventCount; ++idx)
        {
            const int32 hEvent = ppEvents[idx]->GetHandle();
            FD_SET(hEvent, &eventSet);

            maxEventFd = Max(maxEventFd, hEvent);
        }

        // According to the man pages, pselect's first argument is the maxiumum file descriptor in the fd_set, plus one.
        const int32 ret = pselect(maxEventFd + 1, &eventSet, nullptr, nullptr, &spec, nullptr);
        if (ret == 0)
        {
            // Timeout ocurred!
            result = Result::Timeout;
        }
        else if (ret == -1)
        {
            // An unknown error occurred.
            PAL_ALERT_ALWAYS();
            result = Result::ErrorUnknown;
        }
        else
        {
            // One or more of the events has entered the signaled state. We don't care which.
        }
    }
    else
    {
        PAL_NOT_TESTED();   // This codepath has not yet been tested.

        // The more complex case is when we need to wait for all of the supplied events to become signaled.  This is
        // tricky on Linux because there is no call which performs a "wait until all these file descriptors are ready"
        // operation -- we will need to wait in a loop until all of the eventfd objects are ready (or a timeout occurs).

        // First, create a new "timerfd" object, which will be used to determine if a timeout occurs.  A timerfd is a
        // file descriptor which is readable when the timer has expired.

        // NOTE: This is necessary because we will eventually cal epoll_wait() in a loop, and if we explicitly specify a
        //       timeout duration in each call then the total timeout may end up being N times longer than expected
        //       (where N is the number of loop iterations we need before all events are triggered).
        //
        //       An alternative approach would be to use the timeout parameter of epoll_wait(), and after each call
        //       returns without timing-out, we could subtract the duration of the wait call from the initial timeout
        //       duration before invoking epoll_wait() again. It is unclear at this time which approach is more
        //       efficient, so this may change after we are far-enough along where performance analysis on Linux is
        //       possible.
        const int32 timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        if (timerFd == -1)
        {
            PAL_ALERT_ALWAYS();
            result = Result::ErrorUnknown;
        }
        else
        {
            const uint32 maxEvents = (eventCount + 1);

            // Next, create an epoll context, which is used to wait on each of the event objects as well as the timer
            // object.
            const int32 epollFd = epoll_create(maxEvents);
            if (epollFd == -1)
            {
                PAL_ALERT_ALWAYS();
                result = Result::ErrorUnknown;
            }
            else
            {
                // Add each of the eventfd objects to the epoll context. Each eventfd object is polled for readiness to
                // be readable, and will be automatically removed by the system once an event is generated for that
                // object.
                for (uint32 idx = 0; idx < eventCount; ++idx)
                {
                    const int32 hEvent = ppEvents[idx]->GetHandle();

                    epoll_event eventInfo = { };
                    eventInfo.events  = (EPOLLIN | EPOLLONESHOT);
                    eventInfo.data.fd = hEvent;

                    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, hEvent, &eventInfo) != 0)
                    {
                        // Is there any way to recover from this error?
                        PAL_ALERT_ALWAYS();
                        result = Result::ErrorUnknown;
                    }
                }

                // Add the timerfd object to the epoll context. It will be polled for timer expiration and will not be
                // automatically removed by the system once the timer expires.
                epoll_event eventInfo = { };
                eventInfo.events  = EPOLLIN;
                eventInfo.data.fd = timerFd;
                if (epoll_ctl(epollFd, EPOLL_CTL_ADD, timerFd, &eventInfo) != 0)
                {
                    // Is there any way to recover from this error?
                    PAL_ALERT_ALWAYS();
                    result = Result::ErrorUnknown;
                }

                // Next, initialize the timeout duration before the timerfd expires.
                itimerspec duration = {};
                memcpy(&duration.it_value, &spec, sizeof(timespec));
                if (timerfd_settime(timerFd, 0, &duration, nullptr) != 0)
                {
                    // Is there any way to recover from this error?
                    PAL_ALERT_ALWAYS();
                    result = Result::ErrorUnknown;;
                }

                AutoBuffer<epoll_event, 16, Allocator> eventBuffer(maxEvents, pAllocator);
                if (eventBuffer.Capacity() >= maxEvents)
                {
                    uint32 remainingEvents = eventCount;

                    while ((result == Result::Success) && (remainingEvents > 0))
                    {
                        // Wait for one (or more) event or timer objects to become signaled. Each eventfd object is
                        // automatically removed from the epoll context when it becomes signaled.
                        int32 numEvents = epoll_wait(epollFd, &eventBuffer[0], maxEvents, -1);
                        PAL_ASSERT(numEvents != -1);

                        for (int32 ev = 0; ev < numEvents; ++ev)
                        {
                            if (eventBuffer[ev].data.fd == timerFd)
                            {
                                // The timer object has expired... we need to bail out since this is a timeout
                                // condition.
                                result = Result::Timeout;
                                break;
                            }
                            else
                            {
                                // An event object has signaled... decrement the count of remaining events to wait on.
                                PAL_ASSERT(remainingEvents > 0);
                                --remainingEvents;
                            }
                        }

                        // If the timeout condition wasn't hit, the loop will continue waiting on all event objects
                        // which haven't become signaled yet...
                    }
                }
                else
                {
                    result = Result::ErrorOutOfMemory;
                }

                close(epollFd);
            }

            close(timerFd);
        }
    }

    return result;
}

} // Util
