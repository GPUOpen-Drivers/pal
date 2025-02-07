/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  lnxLibrary.cpp
 * @brief PAL utility collection Library class Linux implementation.
 ***********************************************************************************************************************
 */

#include "palLibrary.h"

namespace Util
{

// =====================================================================================================================
// Loads a Shared Object with the specified name into this process.
Result Library::Load(
    const char* pLibraryName)
{
    constexpr uint32 Flags = RTLD_LAZY;
    m_hLib = dlopen(pLibraryName, Flags);

    return (m_hLib == nullptr) ? Result::ErrorUnavailable : Result::Success;
}

// =====================================================================================================================
// Unloads this Shared Object if it was loaded previously.  Called automatically during the object destructor.
void Library::Close()
{
    if (m_hLib != nullptr)
    {
        dlclose(m_hLib);
        m_hLib = nullptr;
    }
}

// =====================================================================================================================
// Intended as an alternative to Close() on Windows platforms because it is unsafe in Windows to unload a DLL when your
// DLL is possible already being unloaded.  On Linux, however, this just calls Close() because that problem doesn't
// exist here.
void Library::ReleaseWithoutClosing()
{
    Close();
}

// =====================================================================================================================
void* Library::GetFunctionHelper(
    const char* pName
    ) const
{
    PAL_ASSERT(m_hLib != nullptr);
    return reinterpret_cast<void*>(dlsym(m_hLib, pName));
}

} // Util
