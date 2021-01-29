/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "stdint.h"

namespace ShaderPerfData
{

/// 32-bit version identifier.
struct Version
{
    uint16_t major;  ///< Major version number.
    uint16_t minor;  ///< Minor version number.
};

/// ShaderHash represents a 128-bit shader hash.
struct ShaderHash
{
    uint64_t lower;   ///< Lower 64-bits of hash
    uint64_t upper;   ///< Upper 64-bits of hash
};

static constexpr Version HeaderVersion = { 1, 0 };

// Header for the performance data file for offline parsing and analysis.
struct PerformanceDataHeader
{
    Version     version;          // Current version of this header structure.
    char        apiShaderType[3]; // Two character string representing the API shader type, plus null terminator.
    ShaderHash  shaderHash;       // 128-bit hash for this shader.
    uint64_t    compilerHash;     // 64-bit Compiler hash for the pipeline this shader is part of.
    size_t      payloadSize;      // Size of the total payload following this header, in bytes.
    uint32_t    numShaderChunks;  // Number of shader chunks in the payload, each with its own header.
};

// Enumeration to indicate what type of header this chunk is.
enum class ChunkType : uint32_t
{
    Shader = 0, // This chunk has shader data.
    Count       // The total number of chunk types.
};

// Header for the per-shader data chunks.
struct PerformanceDataShaderHeader
{
    ChunkType chunkType;        // Required at the beginning of every chunk, describes the type of header this is.
    char      hwShaderType[3];  // Two character string representing the HW shader type, plus null terminator.
    size_t    payloadSize;      // Size of the payload following this header, in bytes.
};

} // ShaderPerfData
