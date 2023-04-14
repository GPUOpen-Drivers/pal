/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palTextWriter.h"

namespace Pal
{

namespace DbgOverlay
{

class Device;
class Image;

static constexpr size_t MaxTextLines      = 24;                // Maximum number of text lines
static constexpr size_t MaxTextLength     = 61;                // Maximum characters per line
static constexpr size_t MaxTextLengthComb = 48;                // Maximum characters per line for combined case
static constexpr size_t BufSize           = MaxTextLength + 1; // String buffer length per line

// =====================================================================================================================
// Defines TextWriter objects, which are used to write text to presentable images before presents.
class TextWriter
{
public:
    TextWriter(Device* pDevice);
    ~TextWriter();

    Result Init();

    void WriteVisualConfirm(const Image&                           dstImage,
                            ICmdBuffer*                            pCmdBuffer,
                            const CmdPostProcessDebugOverlayInfo&  debugOverlayInfo) const;

private:
    Device*const m_pDevice;

    GpuUtil::TextWriter<Platform> m_textWriter;
};

} // DbgOverlay
} // Pal
