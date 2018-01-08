/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palShader.h"
#include "palShaderCache.h"

namespace Pal
{

class Device;

// =====================================================================================================================
// Simple class which acts as a PAL wrapper around an SCPC shader object.  This class is only needed temporarily, and
// can be removed from PAL after the transition to SCPC is complete.
class Shader : public IShader
{
public:
    explicit Shader(const Device& device);

    Result Init(const ShaderCreateInfo& createInfo);

    virtual void Destroy() override;
    void DestroyInternal();

    virtual ShaderType GetType() const override;
    virtual bool UsesPushConstants() const override;

    static size_t GetSize(
        const Device&           device,
        const ShaderCreateInfo& createInfo,
        Result*                 pResult);

private:
    virtual ~Shader() { }

    const Device&   m_device;

    PAL_DISALLOW_DEFAULT_CTOR(Shader);
    PAL_DISALLOW_COPY_AND_ASSIGN(Shader);
};

// =====================================================================================================================
// Simple class which acts as a PAL wrapper around an SCPC shader cache object.  This class is only needed temporarily,
// and can be removed from PAL after the transition to SCPC is complete.
class ShaderCache : public IShaderCache
{
public:
    explicit ShaderCache(const Device& device);

    Result Init(const ShaderCacheCreateInfo& createInfo, bool enableDiskCache);

    virtual void Destroy() override;
    virtual Result Serialize(void* pBlob, size_t* pSize) override;
    virtual Result Merge(uint32 numSrcCaches, const IShaderCache** ppSrcCaches) override;
    virtual void Reset() override;

    ShaderCacheGetValue GetValueFunc() const { return m_pfnGetValue; }
    ShaderCacheStoreValue StoreValueFunc() const { return m_pfnStoreValue; }

    static size_t GetSize(
        const Device&                device,
        const ShaderCacheCreateInfo& createInfo,
        Result*                      pResult);

private:
    virtual ~ShaderCache() { }

    const Device&        m_device;

    ShaderCacheGetValue    m_pfnGetValue;
    ShaderCacheStoreValue  m_pfnStoreValue;

    PAL_DISALLOW_DEFAULT_CTOR(ShaderCache);
    PAL_DISALLOW_COPY_AND_ASSIGN(ShaderCache);
};

} // Pal
