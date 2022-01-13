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

#include "palUuid.h"
#include "palAssert.h"

#include "util/imported/libuuid/libuuid.h"

#include <random>
#include <unistd.h>

namespace Util
{
namespace Uuid
{
namespace Os
{

static constexpr const int32  SecTo100NsFactor                = 10000000L;
static constexpr const uint64 OffsetFromUuidEpochToLinuxEpoch = 122192928000000000ULL;

// =====================================================================================================================
Node GetLocalNode()
{
    Node node;
    bool libuuidGetNodeIdSuccess = (uuid_get_node_id(node.raw) == 1);
    PAL_ASSERT(libuuidGetNodeIdSuccess);

    // If libuuid can't get our host id generate a random number based on gethostid()
    if (libuuidGetNodeIdSuccess == false)
    {
        uint64 id = ::std::mt19937_64{::std::random_device{}()}();
        memcpy(node.raw, &id, 6);
    }

    // Enusre multicast bit is set
    node.raw[0] |= 0x01;

    return node;
}

// =====================================================================================================================
uint64 GetFixedTimePoint()
{
    // February 1, 2021, 00:00:00
    tm FixedPoint = {0, 0, 0, 1, 2, 2021};
    const time_t fixedTimeT = mktime(&FixedPoint);
    const uint64 secondsSinceUuidEpoch = fixedTimeT + OffsetFromUuidEpochToLinuxEpoch;
    const uint64 uuidTimeUnitsSinceUuidEpoch = secondsSinceUuidEpoch * SecTo100NsFactor;

    return uuidTimeUnitsSinceUuidEpoch & 0x0FFFFFFFFFFFFFFFULL;
}

// =====================================================================================================================
uint32_t GetSequenceStart()
{
    timespec time;
    bool clockGettimeSuccess = (clock_gettime(CLOCK_MONOTONIC, &time) == 0);
    PAL_ASSERT(clockGettimeSuccess);

    return time.tv_nsec;
}

// =====================================================================================================================
Timestamp GetCurrentTimestamp()
{
    timespec time;
    bool clockGettimeSuccess = (clock_gettime(CLOCK_REALTIME, &time) == 0);
    PAL_ASSERT(clockGettimeSuccess);

    const uint64 secondsSinceUuidEpoch = time.tv_sec + OffsetFromUuidEpochToLinuxEpoch;
    const uint64 uuidTimeUnitsSinceUuidEpoch =
        (secondsSinceUuidEpoch * SecTo100NsFactor) + (time.tv_nsec / 100);

    return uuidTimeUnitsSinceUuidEpoch & 0x0FFFFFFFFFFFFFFFULL;
}

} // namespace Os
} // namespace Uuid
} // namespace Util
