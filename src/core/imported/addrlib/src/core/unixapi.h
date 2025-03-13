// ============================================================================ //
/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2010-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
// ============================================================================ //

#ifndef CORE_UNIXAPI_H
#define CORE_UNIXAPI_H

// -----------------------------------------------------------------------------

// These are headers which are included by Windows.h. We need to beat windows
// to it so they don't get put in the UnixApi namespace...
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NOMINMAX
#define STRICT
#include <sys/types.h>
#include <errno.h>

namespace UnixApi {

typedef int HKEY;

#define HKEY_CLASSES_ROOT 0
#define HKEY_CURRENT_USER 1
#define HKEY_LOCAL_MACHINE 2
#define HKEY_USERS 3

static HKEY const HKeyClassesRoot = HKEY_CLASSES_ROOT;
static HKEY const HKeyCurrentUser = HKEY_CURRENT_USER;
static HKEY const HKeyLocalMachine = HKEY_LOCAL_MACHINE;
static HKEY const HKeyUsers = HKEY_USERS;

}; // namespace UnixApi

// -----------------------------------------------------------------------------

#endif

