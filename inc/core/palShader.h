/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palShader.h
 * @brief Defines the Platform Abstraction Library (PAL) IShader interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palDestroyable.h"

/// The major version of AMD IL that PAL can parse correctly. Shaders compiled with a larger major version may not be
/// parsed appropriately.
#define PAL_SUPPORTED_IL_MAJOR_VERSION 2

namespace Pal
{

/// ShaderHash represents a 128-bit shader hash.
struct ShaderHash
{
    uint64 lower;   ///< Lower 64-bits of hash
    uint64 upper;   ///< Upper 64-bits of hash
};

/// Determines whether two ShaderHashes are equal.
///
/// @param  [in]    hash1    The first 128-bit shader hash
/// @param  [in]    hash2    The second 128-bit shader hash
///
/// @returns True if the shader hashes are equal.
PAL_INLINE bool ShaderHashesEqual(
    const ShaderHash hash1,
    const ShaderHash hash2)
{
    return ((hash1.lower == hash2.lower) & (hash1.upper == hash2.upper));
}

/// Determines whether the given ShaderHash is non-zero.
///
/// @param  [in]    hash    A 128-bit shader hash
///
/// @returns True if the shader hash is non-zero.
PAL_INLINE bool ShaderHashIsNonzero(
    const ShaderHash hash)
{
    return ((hash.upper | hash.lower) != 0);
}

/// Specifies a shader type (i.e., what stage of the pipeline this shader was written for).
enum class ShaderType : uint32
{
    Compute = 0,
    Vertex,
    Hull,
    Domain,
    Geometry,
    Pixel,
};

/// Number of shader program types supported by PAL.
constexpr uint32 NumShaderTypes =
    (1u + static_cast<uint32>(ShaderType::Pixel) - static_cast<uint32>(ShaderType::Compute));

} // Pal
