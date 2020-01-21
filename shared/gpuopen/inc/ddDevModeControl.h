/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <gpuopen.h>
#include <ddPlatform.h>

namespace DevDriver
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generate an authentication token for the data using the salt provided
// Currently this generates a CRC32 of the data using a slightly mangled salt
inline uint32 GenerateAuthToken(uint32 salt, const void* pData, uint32 length)
{
    return CRC32(pData, length, salt ^ ~kMessageVersion);
}

///////////////////////
// Typedef for the Router Prefix type
typedef uint32 RouterPrefix;

enum struct DevModeCmd: uint32
{
    Unknown = 0,                // Illegal command

    RegisterClient,             // Register a new client on the bus
    UnregisterClient,           // Unregister an existing client from the bus

    RegisterRouter,             // Register a new router on the bus
    UnregisterRouter,           // Unregister an existing router from the bus

    EnableDeveloperMode,        // Attempts to enable developer mode on the bus
    DisableDeveloperMode,       // Attempts to disable developer mode on the bus

    QueryCapabilities,          // Queries the capabilities of the bus
    QueryDeveloperModeStatus,   // Queries the current developer mode configuration

    Count,
};

/// DevMode Request Header
/// All DevMode command types have this header at the beginning of the struct.
DD_NETWORK_STRUCT(DevModeResponseHeader, 4)
{
    DevModeCmd cmd       = DevModeCmd::Unknown; /// The devmode command to be executed
    Result     result    = Result::Error;       /// The result of the devmode command execution

    uint32     reserved1 = 0;                   /// Reserved for future use (Program to zero)
    uint32     reserved0 = 0;                   /// Reserved for future use (Program to zero)

    static DevModeResponseHeader FromCmd(DevModeCmd devModeCmd)
    {
        DevModeResponseHeader header;
        header.cmd = devModeCmd;

        return header;
    }
};

DD_CHECK_SIZE(DevModeResponseHeader, 16);

enum struct DevModeBusType: uint32
{
    Unknown = 0, // Unknown

    Auto,        // Automatic selection
    UserMode,    // Request a user mode bus
    KernelMode,  // Request a kernel mode bus

    Count
};

static inline const char* DevModeCmdToHumanString(DevModeCmd cmd)
{
    switch(cmd)
    {
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

///////////////////////
// Developer Mode Status Flags
union DeveloperModeFlags
{
    struct {
        uint32 enableEmbeddedClient : 1;  // Enable the embedded client
        uint32 enableTdrLogging     : 1;  // Enable TDR Logging
                                          // This only is used if the embedded client is available
        uint32 reserved             : 30; // Reserved for future use
    } flags;
    uint32 u32All;

    constexpr DeveloperModeFlags()
        : u32All(0)
    {
    }

};

///////////////////////
// Developer Mode Initialization Settings
DD_NETWORK_STRUCT(DeveloperModeSettings, 4)
{
    RouterPrefix       routerPrefix; // Routing prefix to be assigned by the router
    DeveloperModeFlags features;     // Developer Mode initialization flags
};

}
