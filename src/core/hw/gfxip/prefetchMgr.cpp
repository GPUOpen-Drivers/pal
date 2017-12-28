/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/g_palSettings.h"
#include "core/hw/gfxip/prefetchMgr.h"

namespace Pal
{

static_assert(((ShaderType::Compute  == static_cast<ShaderType>(PrefetchCs)) &&
               (ShaderType::Vertex   == static_cast<ShaderType>(PrefetchVs)) &&
               (ShaderType::Hull     == static_cast<ShaderType>(PrefetchHs)) &&
               (ShaderType::Domain   == static_cast<ShaderType>(PrefetchDs)) &&
               (ShaderType::Geometry == static_cast<ShaderType>(PrefetchGs)) &&
               (ShaderType::Pixel    == static_cast<ShaderType>(PrefetchPs))),
              "Shader types do not match prefetch types!");

// =====================================================================================================================
PrefetchMgr::PrefetchMgr(
    const GfxDevice& device)
    :
    m_device(device),
    m_curPrefetchMask(AllShaderPrefetchMask),
    m_dirtyFlags(0),
    m_shaderPrefetchMask(m_curPrefetchMask & AllShaderPrefetchMask)
{
}

// =====================================================================================================================
// Enables/disables shader prefetching.  Should be called when a command buffer is begun based on e.g. client-dependent
// command buffer optimization flags.
void PrefetchMgr::EnableShaderPrefetch(
    bool enable)  // True enables shader prefetching; false disables it.
{
    if (enable == true)
    {
        m_curPrefetchMask |= m_shaderPrefetchMask;
    }
    else
    {
        m_curPrefetchMask &= ~m_shaderPrefetchMask;
    }
}

} // Pal
