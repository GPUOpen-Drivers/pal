/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "ndFence.h"

using namespace Util;

namespace Pal
{
namespace NullDevice
{
// =====================================================================================================================
Result Fence::Init(
    const FenceCreateInfo& createInfo)
{
    return Result::Success;
}

// =====================================================================================================================
OsExternalHandle Fence::ExportExternalHandle(
    const FenceExportInfo& exportInfo) const
{
    return -1;
}

// =====================================================================================================================
Result Fence::Reset()
{
    return Result::Success;
}

// =====================================================================================================================
Result Fence::WaitForFences(
    const Pal::Device&      device,
    uint32                  fenceCount,
    const Pal::Fence*const* ppFenceList,
    bool                    waitAll,
    uint64                  timeout) const
{
    return Result::Success;
}

} //NullDevice
} // Pal
