/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

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
    void           Reset();

private:
    uint64               m_timestampFrequency;
    uint64               m_lastTimestamp;
    Platform::AtomicLock m_lastTimestampLock;
};

} // namespace DevDriver
