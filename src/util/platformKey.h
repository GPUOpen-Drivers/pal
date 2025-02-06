/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palPlatformKey.h"
#include "palHashProvider.h"

namespace Util
{

// =====================================================================================================================
// Platform key generator implementation
class PlatformKey : public IPlatformKey
{
public:
    PlatformKey(
        IHashContext*  pKeyContext,
        uint8*         pKeyData,
        void*          pTempContextMem);
    ~PlatformKey();

    virtual size_t GetKeySize() const final { return m_pKeyContext->GetOutputBufferSize(); }

    virtual const uint8* GetKey() const final { return m_pKeyData; }

    virtual uint64 GetKey64() const final { return m_keyData64; }

    virtual Result AppendClientData(
        const void* pData,
        size_t      dataSize) final;

    virtual const IHashContext* GetKeyContext() const final { return m_pKeyContext; }

    virtual void Destroy() final { this->~PlatformKey(); }

private:
    PAL_DISALLOW_DEFAULT_CTOR(PlatformKey);
    PAL_DISALLOW_COPY_AND_ASSIGN(PlatformKey);

    Result RecalcKey();

    IHashContext* const  m_pKeyContext;
    uint8* const         m_pKeyData;
    uint64               m_keyData64;
    void* const          m_pTempContextMem;
};

} //namespace Util
