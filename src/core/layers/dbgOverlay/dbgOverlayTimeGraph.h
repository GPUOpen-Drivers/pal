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

#include "palTimeGraph.h"

namespace Pal
{

namespace DbgOverlay
{

class Device;
class Image;

// =====================================================================================================================
// Defines TimeGraph objects, which are used to draw to presentable images before presents.
class TimeGraph
{
public:
    TimeGraph(Device* pDevice);
    ~TimeGraph();

    Result Init();

    void DrawVisualConfirm(
        const Image&           dstImage,    // Image to write visual confirm into.
        ICmdBuffer*            pCmdBuffer,  // Command buffer to write commands into.
        const UniquePresentKey presentKey) const;

private:
    Device*const                 m_pDevice;
    GpuUtil::TimeGraph<Platform> m_timegraph;
};

} // DbgOverlay
} // Pal
