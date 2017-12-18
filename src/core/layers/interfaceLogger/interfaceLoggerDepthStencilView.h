/*
 *******************************************************************************
 *
 * Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All rights reserved.
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

#pragma once

#include "core/layers/decorators.h"

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
class DepthStencilView : public DepthStencilViewDecorator
{
public:
    DepthStencilView(IDepthStencilView* pNextView, const DeviceDecorator* pDevice, uint32 objectId)
        : DepthStencilViewDecorator(pNextView, pDevice), m_objectId(objectId) { }

    // Returns this object's unique ID.
    uint32 ObjectId() const { return m_objectId; }

private:
    virtual ~DepthStencilView() { }

    const uint32 m_objectId;

    PAL_DISALLOW_DEFAULT_CTOR(DepthStencilView);
    PAL_DISALLOW_COPY_AND_ASSIGN(DepthStencilView);
};

} // InterfaceLogger
} // Pal
