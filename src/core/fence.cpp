/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/fence.h"
#include "core/platform.h"
#include "core/queue.h"
#include "palMutex.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
Fence::Fence()
{
    m_fenceState.flags = 0;
    m_fenceState.neverSubmitted = 1;
}

// =====================================================================================================================
// Destroys this Fence object. Clients are responsible for freeing the system memory the object occupies.
// NOTE: Part of the public IDestroyable interface.
void Fence::Destroy()
{
    this->~Fence();
}

// =====================================================================================================================
// Destroys an internal fence object: invokes the destructor and frees the system memory block it resides in.
void Fence::DestroyInternal(
    Platform* pPlatform)
{
    Destroy();
    PAL_FREE(this, pPlatform);
}

} // Pal
