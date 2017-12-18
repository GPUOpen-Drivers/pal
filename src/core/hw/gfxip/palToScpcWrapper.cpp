/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
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

#include "core/device.h"
#include "core/hw/gfxip/palToScpcWrapper.h"
#include "palAutoBuffer.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{

static const char CacheFileSubPath[] = "/.AMD/PalCache/";

static const char ClientStr[] = "XGL";

// =====================================================================================================================
size_t Shader::GetSize(
    const Device&           device,
    const ShaderCreateInfo& createInfo,
    Result*                 pResult)
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::ErrorUnavailable;
    }

    return 0;
}

// =====================================================================================================================
Shader::Shader(
    const Device& device)
    :
    m_device(device)
{
}

// =====================================================================================================================
Result Shader::Init(
    const ShaderCreateInfo& createInfo)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
void Shader::Destroy()
{

    this->~Shader();
}

// =====================================================================================================================
void Shader::DestroyInternal()
{
    Platform*const pPlatform = m_device.GetPlatform();
    Destroy();
    PAL_FREE(this, pPlatform);
}

// =====================================================================================================================
ShaderType Shader::GetType() const
{
    return ShaderType::Compute;
}

// =====================================================================================================================
bool Shader::UsesPushConstants() const
{
    return false;
}

// =====================================================================================================================
size_t ShaderCache::GetSize(
    const Device&                device,
    const ShaderCacheCreateInfo& createInfo,
    Result*                      pResult)
{
    if (pResult != nullptr)
    {
        (*pResult) = Result::ErrorUnavailable;
    }

    return 0;
}

// =====================================================================================================================
ShaderCache::ShaderCache(
    const Device& device)
    :
    m_device(device),
    m_pfnGetValue(nullptr),
    m_pfnStoreValue(nullptr)
{
}

// =====================================================================================================================
Result ShaderCache::Init(
    const ShaderCacheCreateInfo& createInfo,
    bool                         enableDiskCache)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
void ShaderCache::Destroy()
{

    this->~ShaderCache();
}

// =====================================================================================================================
Result ShaderCache::Serialize(
    void*   pBlob,
    size_t* pSize)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
Result ShaderCache::Merge(
    uint32               numSrcCaches,
    const IShaderCache** ppSrcCaches)
{
    return Result::ErrorUnavailable;
}

// =====================================================================================================================
void ShaderCache::Reset()
{
}

} // Pal
