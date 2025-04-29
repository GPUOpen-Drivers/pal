/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
/// @author AMD Developer Tools Team
/// @file
/// @brief Definitions for Driver Override RDF chunk.
//=============================================================================

#ifndef DRIVER_OVERRIDES_UTILS_SOURCE_DRIVER_OVERRIDES_DEFINITIONS_H_
#define DRIVER_OVERRIDES_UTILS_SOURCE_DRIVER_OVERRIDES_DEFINITIONS_H_

namespace driver_overrides_utils
{
    static constexpr const char* kDriverOverridesChunkIdentifier = "DriverOverrides";
    static constexpr uint32_t    kDriverOverridesChunkVersion    = 3;                             ///< Current Driver Override chunk version.
    static constexpr uint32_t    kDriverOverridesChunkVersionMin = 2;                             ///< Minimum supported chunk version.
    static constexpr uint32_t    kDriverOverridesChunkVersionMax = kDriverOverridesChunkVersion;  ///< Maximum supported chunk version.

    static constexpr const char* kDriverOverridesmiscellaneousStructure = "Misc.";  ///< String used for unnamed structures.

    static constexpr const char* kNodeStringIsDriverExperiments = "IsDriverExperiments";
    static constexpr const char* kNodeStringComponents          = "Components";
    static constexpr const char* kNodeStringComponent           = "Component";
    static constexpr const char* kNodeStringStructures          = "Structures";
    static constexpr const char* kNodeStringStructure           = "Structure";
    static constexpr const char* kNodeStringSettingName         = "SettingName";
    static constexpr const char* kNodeStringCurrent             = "Current";
    static constexpr const char* kNodeStringValue               = "Value";
    static constexpr const char* kNodeStringUserOverride        = "UserOverride";
    static constexpr const char* kNodeStringDescription         = "Description";
    static constexpr const char* kNodeStringSupported           = "Supported";

}  // namespace driver_overrides_utils
#endif
