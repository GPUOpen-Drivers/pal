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
* @file  ddDevModeControl.h
* @brief Cross platform interface to utility driver
***********************************************************************************************************************
*/
#pragma once

#include "gpuopen.h"

namespace DevDriver
{
enum struct DevModeCmd: uint32
{
    Unknown = 0,                // Illegal command
    Ping    = 1,                // Ping to check that the kernel module is still there

    // Unimplemented commands
    RegisterClient,             //
    UnregisterClient,           //

    RegisterRouter,             //
    UnregisterRouter,           //

    EnableDeveloperMode,        //
    DisableDeveloperMode,       //

    QueryCapabilities,          //
    QueryDeveloperModeStatus,   //

    Count,
};

static inline const char* DevModeCmdToHumanString(DevModeCmd cmd)
{
    switch(cmd)
    {
        case DevModeCmd::Unknown:                  return "Unknown";
        case DevModeCmd::Ping:                     return "Ping";

        case DevModeCmd::RegisterClient:           return "RegisterClient";
        case DevModeCmd::UnregisterClient:         return "UnregisterClient";

        case DevModeCmd::RegisterRouter:           return "RegisterRouter";
        case DevModeCmd::UnregisterRouter:         return "UnregisterRouter";

        case DevModeCmd::EnableDeveloperMode:      return "EnableDeveloperMode";
        case DevModeCmd::DisableDeveloperMode:     return "DisableDeveloperMode";

        case DevModeCmd::QueryCapabilities:        return "QueryCapabilities";
        case DevModeCmd::QueryDeveloperModeStatus: return "QueryDeveloperModeStatus";

        default:                                   return "<Unrecognized DevModeCmd>";
    }
}

// Common header that all devmode cmd output types embed.
struct DevModeCmdOutputHeader
{
    // Result of executing the command, or a description of why it wasn't executed.
    Result cmdResult = Result::Error;

    // The total byte count written out by the devmode.
    uint64 bytesWritten = 0;
};

struct DevModeCmdPingInput
{
    uint32 pingValue = 0;
};

struct DevModeCmdPingOutput
{
    DevModeCmdOutputHeader header    = {};
    uint32                 pongValue = 0;
};

/// Configuration for creating a IDevModeDevice
struct DevModeDeviceCreateInfo
{
    union
    {
        struct
        {
            uint32 enableTestDriver : 1;  /// Load a testing driver in usermode instead of the platform default.
                                          /// If this is not supported, CreateDevModeDevice() will return an error.
            uint64 reserved         : 63; /// Program to zero.
        };
        uint64 value;
    } flags;
};

/// Abstract interface to platform-specific interface to a utility driver.
class IDevModeDevice
{
public:
    IDevModeDevice() {}
    virtual ~IDevModeDevice() {}

    DD_NODISCARD
    virtual Result Destroy() = 0;

    /// Platform-agnostic call into the devmode device.
    ///
    /// Prefer calling the typed-wrapper instead of this whenever possible.
    DD_NODISCARD
    virtual Result MakeDevModeRequest(
        DevModeCmd cmd,
        size_t     inputSize,
        void*      pInput,
        size_t     outputSize,
        void*      pOutput
    ) = 0;

    /// Platform-agnostic call into the devmode device.
    ///
    /// This is a convience overload to prevent errors with Input and Output types.
    template <typename Input, typename Output>
    DD_NODISCARD
    Result MakeDevModeRequest(
        DevModeCmd cmd,
        Input*     pInput,
        Output*    pOutput
    )
    {
        // TODO: Any other invariants that we want here?
        static_assert(
            (offsetof(Output, header) == 0) && (sizeof(pOutput->header) == sizeof(DevModeCmdOutputHeader)),
            "Output types MUST embed a DevModeCmdOutputHeader named \"header\" at offset 0."
        );
        return MakeDevModeRequest(
            cmd,
            sizeof(*pInput),
            pInput,
            sizeof(*pOutput),
            pOutput
        );
    }
};

// Create the appropriate platform-dependent device.
Result CreateDevModeDevice(
    const DevModeDeviceCreateInfo& createInfo,
    IDevModeDevice** ppOutDevice
);

}
