/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <util/ddEventTimer.h>

namespace DevDriver
{

//=====================================================================================================================
EventTimer::EventTimer()
    : m_timestampFrequency(Platform::QueryTimestampFrequency())
    , m_lastTimestamp(0)
{
}

//=====================================================================================================================
EventTimer::~EventTimer()
{
}

//=====================================================================================================================
EventTimestamp EventTimer::CreateTimestamp()
{
    EventTimestamp eventTimestamp = {};

    // Acquire a lock to control access to our last timestamp value
    m_lastTimestampLock.Lock();

    const uint64 timestamp = Platform::QueryTimestamp();
    const uint64 deltaSinceLastToken = ((timestamp - m_lastTimestamp) / kEventTimeUnit);

    const bool needsFullTimestamp = ((deltaSinceLastToken > kEventTimestampThreshold) || (m_lastTimestamp == 0));
    const bool needsTimeDelta     = (deltaSinceLastToken > kEventTimeDeltaThreshold);

    // If enough time has passed, we'll need to output either a full timestamp or a time delta timestamp.
    // In either case, we need to update our last timestamp.
    if (needsFullTimestamp || needsTimeDelta)
    {
        m_lastTimestamp = timestamp;
    }

    m_lastTimestampLock.Unlock();

    if (needsFullTimestamp)
    {
        // In this case we need to write a timestamp and the delta returned will be zero
        eventTimestamp.type           = EventTimestampType::Full;
        eventTimestamp.full.timestamp = (timestamp / kEventTimeUnit);
        eventTimestamp.full.frequency = m_timestampFrequency;
    }
    else if (needsTimeDelta)
    {
        // In this case we need to write a large delta
        eventTimestamp.type             = EventTimestampType::LargeDelta;
        eventTimestamp.largeDelta.delta = deltaSinceLastToken;

        // Count the number of bytes in the delta
        uint64 numBytes = 1;
        while (((1ull << (numBytes * 8)) - 1) < deltaSinceLastToken)
        {
            ++numBytes;
        }

        DD_ASSERT(numBytes <= 6);
        eventTimestamp.largeDelta.numBytes = static_cast<uint8>(numBytes);
    }
    else
    {
        // In this case, the time elapsed since the last full timestamp/time delta is small enough that we can just
        // calculate and return the delta
        const uint8 delta               = static_cast<uint8>(deltaSinceLastToken / kEventTimeUnit);
        eventTimestamp.type             = EventTimestampType::SmallDelta;
        eventTimestamp.smallDelta.delta = delta;
    }

    return eventTimestamp;
}

void EventTimer::Reset()
{
    // Acquire a lock since we modify our last timestamp here
    Platform::LockGuard<Platform::AtomicLock> lockGuard(m_lastTimestampLock);

    m_lastTimestamp = 0;
}

} // namespace DevDriver
