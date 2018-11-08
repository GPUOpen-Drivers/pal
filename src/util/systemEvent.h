/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palSystemEvent.h"

namespace Util
{

// String table of Debug Print strings for events
// Will be initialized in the autogen code based on the string given from the config file
// Will have multiple OS definitions because config files will be different per os
extern char* g_dpfEventFmtTable[];

// Initialization function that needs to be called before LogSystemEvent() is used to emit an OS event.
// This is used to register the OS event emitter with the operating system.
// This function has a multiple OS dependent implementations.
extern void SystemEventInit();

// Cleanup function that needs to be called in order to unregister the provider for event tracing
// This function has a multiple OS dependent implementations.
extern void SystemEventDestroy();

// Checks the client mode in the client array and returns it
extern uint32 CheckClientMode(
    SystemEventClientId id);

} // Util
