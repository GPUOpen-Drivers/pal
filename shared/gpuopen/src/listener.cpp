/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
***********************************************************************************************************************
* @file listener.cpp
* @brief Implementation file for listener.
***********************************************************************************************************************
*/

#include "ddPlatform.h"
#include "listener.h"
#include "../listener/listenerCore.h"

namespace DevDriver
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Create a listener object
    Result CreateListener(const ListenerCreateInfo& createInfo, IListener** ppListener)
    {
        Result result = Result::InvalidParameter;

        ListenerCore* pListenerCore = nullptr;

        if (ppListener != nullptr)
        {
            result = Result::InsufficientMemory;

            // Make sure we have reasonable allocator functions before we try to use them
            DD_ASSERT(createInfo.allocCb.pfnAlloc != nullptr);
            DD_ASSERT(createInfo.allocCb.pfnFree != nullptr);

            pListenerCore = DD_NEW(ListenerCore, createInfo.allocCb);

            if (pListenerCore != nullptr)
            {
                result = pListenerCore->Initialize(createInfo);
            }

            if (result != Result::Success)
            {
                DD_DELETE(pListenerCore, createInfo.allocCb);
            }
        }

        if (result == Result::Success)
        {
            *ppListener = pListenerCore;
        }

        return result;
    }
}
