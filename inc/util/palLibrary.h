/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palLibrary.h
 * @brief PAL utility collection Library class declaration.
 ***********************************************************************************************************************
 */

#pragma once

#include "palAssert.h"
#include "palUtil.h"

#   include <dlfcn.h>

namespace Util
{

/**
***********************************************************************************************************************
* @brief  Abstracts loading dynamic libraries and accessing public functions from them.
***********************************************************************************************************************
*/
class Library
{
    typedef void*   LibraryHandle;

public:
    Library() : m_hLib(nullptr) { }
    ~Library() { Close(); }

    Result Load(const char* pLibraryName);

    void Close();
    void ReleaseWithoutClosing();

    bool IsLoaded() const { return (m_hLib != nullptr); }

    void Swap(Library* pLibrary)
    {
        PAL_ASSERT(pLibrary != nullptr);
        m_hLib           = pLibrary->m_hLib;
        pLibrary->m_hLib = nullptr;
    }

    // Retrieve a function address from the dynamic library object. Returns true if successful, false otherwise.
    template <typename Func_t>
    bool GetFunction(const char* pName, Func_t** ppFunction) const
    {
        (*ppFunction) = reinterpret_cast<Func_t*>(GetFunctionHelper(pName));
        return ((*ppFunction) != nullptr);
    }

private:
    void* GetFunctionHelper(const char* pName) const;

    LibraryHandle  m_hLib;

    PAL_DISALLOW_COPY_AND_ASSIGN(Library);
};

} // Util
