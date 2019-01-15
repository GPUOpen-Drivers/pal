/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palPipeline.h"

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 408
/// Starting with interface 408.0, it is an error to include palShader.h because we want to remove it.  This file will
/// be removed once interfaces older than 408.0 are no longer supported by PAL.
#error "Fatal error! palShader.h is deprecated and the client is still including it."
#else
#if !defined(PAL_SUPPORTED_IL_MAJOR_VERSION)
/// The major version of AMD IL that PAL can parse correctly. Shaders compiled with a larger major version may not be
/// parsed appropriately.
#define PAL_SUPPORTED_IL_MAJOR_VERSION 2
#endif
#endif
