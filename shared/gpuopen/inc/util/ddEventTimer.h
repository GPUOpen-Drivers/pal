/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddEventTimer.h
* @brief Utility class for event timing logic
***********************************************************************************************************************
*/

#pragma once

#include <ddPlatform.h>

namespace DevDriver
{

// This is the number of clocks timestamps will be expressed as
DD_STATIC_CONST uint64 kEventTimeUnit = 32;

// The threshold for a timestamp delta to trigger a delta token.  Each token has 4 bits of delta.
DD_STATIC_CONST uint64 kEventTimeDeltaThreshold = ((1ull << 4) - 1);

// The threshold for a full timestamp token.  After 6 bytes of time delta the size of the output token is greater
// than or equal to a full timestamp token so it becomes better to output a full timestamp at that threshold.
DD_STATIC_CONST uint64 kEventTimestampThreshold = ((1ull << 48) - 1);

enum class EventTimestampType : uint32
{
    Full = 0,
    LargeDelta,
    SmallDelta,

    Count
};

struct EventTimestamp
{
    EventTimestampType type;

    union
    {
        struct
        {
            uint64 timestamp;
            uint64 frequency;
        } full;

        struct
        {
            uint64 delta;
            uint8  numBytes;
        } largeDelta;

        struct
        {
            uint8 delta;
        } smallDelta;
    };
};

class EventTimer
{
public:
    EventTimer();
    ~EventTimer();

    EventTimestamp CreateTimestamp();
    void           Reset() { m_lastTimestamp = 0; }

private:
    uint64 m_timestampFrequency;
    uint64 m_lastTimestamp;
};

} // namespace DevDriver
