/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "metrohash64.h"
#include "palMsaaState.h"

namespace Pal
{

// =====================================================================================================================
// GFXIP-independent msaa state implementation. See IMsaaState documentation for more details.
class MsaaState : public IMsaaState
{
public:
    virtual void Destroy() override { this->~MsaaState(); }

    uint64 GetStableHash() const { return m_stableHash; }

protected:
    MsaaState(
        const MsaaStateCreateInfo& createInfo)
    {
        Util::MetroHash64::Hash(reinterpret_cast<const uint8*>(&createInfo),
                                sizeof(createInfo),
                                reinterpret_cast<uint8*>(&m_stableHash));
    }
    virtual ~MsaaState() {}

    uint64 m_stableHash;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(MsaaState);
};

} // Pal
