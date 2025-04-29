/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palShaderLibrary.h"
#include "palSpan.h"

namespace Pal
{

class ShaderLibrary;

using ShaderLibrarySpan = Util::Span<const ShaderLibrary* const>;

// =====================================================================================================================
// Base class for ArchiveLibrary and ShaderLibrary
class ShaderLibraryBase : public IShaderLibrary
{
public:
    // Returns array of underlying singleton ShaderLibraries. If this is already a singleton ShaderLibrary,
    // just returns a pointer to itself.
    virtual ShaderLibrarySpan GetShaderLibraries() const = 0;

protected:
    ShaderLibraryBase() = default;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ShaderLibraryBase);
};
} // namespace Pal
