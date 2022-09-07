/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddRpcServer.h>

namespace DriverUtils
{

class IDriverUtilsService
{
public:
    virtual ~IDriverUtilsService() {}

    static const DDRpcServerRegisterServiceInfo kServiceInfo;

    // Informs driver we are collecting trace data
    virtual DD_RESULT EnableTracing() = 0;

    // Informs driver to enable crash analysis mode
    virtual DD_RESULT EnableCrashAnalysisMode() = 0;

protected:
    IDriverUtilsService() {}
};

DD_RESULT RegisterService(DDRpcServer hServer, IDriverUtilsService* pService);

} // namespace DriverUtils
