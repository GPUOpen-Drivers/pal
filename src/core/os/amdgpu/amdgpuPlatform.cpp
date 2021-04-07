/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/amdgpu_asic.h"
#include "core/os/amdgpu/amdgpuPlatform.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuScreen.h"

#include "palSysMemory.h"
#include "palSysUtil.h"
#include "palFile.h"
#include "palFormatInfo.h"
#include "palSettingsFileMgrImpl.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Get the size of an AMDGPU Platform object.
size_t Platform::GetSize()
{
    return sizeof(Amdgpu::Platform);
}

// =====================================================================================================================
// Factory function which instantiates a new AMDGPU Platform object.
Platform* Platform::CreateInstance(
    const PlatformCreateInfo&   createInfo,
    const Util::AllocCallbacks& allocCb,
    void*                       pPlacementAddr)
{
    return PAL_PLACEMENT_NEW(pPlacementAddr) Amdgpu::Platform(createInfo, allocCb);
}

namespace Amdgpu
{

// =====================================================================================================================
Platform::Platform(
    const PlatformCreateInfo&   createInfo,
    const Util::AllocCallbacks& allocCb)
    :
    Pal::Platform(createInfo, allocCb)
{
    m_features.u32All = 0;
}

// =====================================================================================================================
void Platform::Destroy()
{
    TearDownDevices();

    this->~Platform();
}

// =====================================================================================================================
// Get the size to reserve for the PAL screen object on Linux. But it does not exist yet.
size_t Platform::GetScreenObjectSize() const
{
    // Screen objects are not implemented for Linux. Return a nonzero dummy memory size to prevent asserts from firing
    // during initialization.
    return sizeof(Amdgpu::Screen);
}

// =====================================================================================================================
// Initializes PAL's connection to the host OS interface for communicating with the amdgpu driver.
Result Platform::ConnectToOsInterface()
{
    // NOTE: There is nothing to be done here.

    m_features.suportHostMappedForeignMemory = 0;

#if defined(PAL_DEBUG_PRINTS)
    char  executableNameBuffer[256];
    char* pExecutableName;
    // get the application name
    Util::GetExecutableName(&executableNameBuffer[0], &pExecutableName, sizeof(executableNameBuffer));

    // get the time string
    time_t rawTime;
    time(&rawTime);
    struct tm* pTimeInfo = localtime(&rawTime);
    char dateTimeBuffer[64];
    strftime(&dateTimeBuffer[0], sizeof(dateTimeBuffer), "%Y-%m-%d_%H.%M.%S", pTimeInfo);

    // create directory
    Util::Snprintf(m_logPath, sizeof(m_logPath), "/tmp/amdpal/%s_%s",pExecutableName, dateTimeBuffer);
    Util::MkDirRecursively(m_logPath);
#endif

    const DrmLoaderFuncs&  drmProcs  = GetDrmLoader().GetProcsTable();

    if (drmProcs.pfnAmdgpuCsCreateSemisValid()  &&
        drmProcs.pfnAmdgpuCsDestroySemisValid() &&
        drmProcs.pfnAmdgpuCsWaitSemisValid()    &&
        drmProcs.pfnAmdgpuCsSignalSemisValid()  &&
        drmProcs.pfnAmdgpuCsExportSemisValid()  &&
        drmProcs.pfnAmdgpuCsImportSemisValid())
    {
        m_features.supportProSemaphore = 1;
    }

    if (drmProcs.pfnAmdgpuCsCreateSyncobjisValid()  &&
        drmProcs.pfnAmdgpuCsDestroySyncobjisValid() &&
        drmProcs.pfnAmdgpuCsExportSyncobjisValid()  &&
        drmProcs.pfnAmdgpuCsImportSyncobjisValid()  &&
        drmProcs.pfnAmdgpuCsSyncobjExportSyncFileisValid() &&
        drmProcs.pfnAmdgpuCsSyncobjImportSyncFileisValid() &&
        drmProcs.pfnAmdgpuCsSubmitRawisValid())
    {
        m_features.supportSyncObj = 1;
    }

    if (drmProcs.pfnAmdgpuCsCreateSyncobj2isValid())
    {
        m_features.supportCreateSignaledSyncobj = 1;
    }

    if (drmProcs.pfnAmdgpuCsSyncobjWaitisValid()  &&
        drmProcs.pfnAmdgpuCsSyncobjResetisValid() &&
        drmProcs.pfnAmdgpuCsSyncobjSignalisValid())
    {
        m_features.supportSyncobjFence = 1;
    }

    if (drmProcs.pfnAmdgpuCsSubmitRawisValid())
    {
        m_features.supportRawSubmitRoutine = 1;
    }

    if (drmProcs.pfnAmdgpuCsCtxCreate2isValid())
    {
        m_features.supportQueuePriority = 1;
    }

    if (drmProcs.pfnAmdgpuCsCtxCreate3isValid())
    {
        m_features.supportQueueIfhKmd = 1;
    }

    return Result::Success;
}

// =====================================================================================================================
// Enumerates all devices actived by the kernel device driver.
// This method may be called multiple times, because clients will use it to re-enumerate devices after a Device lost
// error occurs.
Result Platform::ReQueryDevices()
{
    Result result = Result::ErrorUnknown;

    const DrmLoaderFuncs& drmProcs    = GetDrmLoader().GetProcsTable();

    drmDevicePtr pDevices[MaxDevices] = { };

    int32 deviceCount = 0;

    if (drmProcs.pfnDrmGetDevicesisValid() && drmProcs.pfnDrmFreeDevicesisValid())
    {
        // If libdrm.so.2 is not successfully resolved, the deviceCount will always be 0.
        deviceCount = drmProcs.pfnDrmGetDevices(pDevices, MaxDevices);
        if (deviceCount < 0)
        {
            result = Result::ErrorInitializationFailed;
        }
    }

    for (int32 i = 0; i < deviceCount; i++)
    {
        char    busId[MaxBusIdStringLen] = {};
        Device* pDevice = nullptr;

        // Check if the device vendor is AMD
        if (!AMDGPU_VENDOR_IS_AMD(pDevices[i]->deviceinfo.pci->vendor_id))
        {
            continue;
        }

        Util::Snprintf(busId,
                       MaxBusIdStringLen,
                       "pci:%04x:%02x:%02x.%d",
                       pDevices[i]->businfo.pci->domain,
                       pDevices[i]->businfo.pci->bus,
                       pDevices[i]->businfo.pci->dev,
                       pDevices[i]->businfo.pci->func);

        result = Device::Create(this,
                                &m_settingsPath[0],
                                busId,
                                pDevices[i]->nodes[DRM_NODE_PRIMARY],
                                pDevices[i]->nodes[DRM_NODE_RENDER],
                                *pDevices[i]->businfo.pci,
                                m_deviceCount,
                                &pDevice);
        if (result == Result::Success)
        {
            m_pDevice[m_deviceCount] = pDevice;
            ++m_deviceCount;
        }
        else if (result == Result::Unsupported)
        {
            PAL_SAFE_DELETE(pDevice, this);
            result = Result::Success;
        }
        else
        {
            PAL_SAFE_DELETE(pDevice, this);
            break;
        }
    }

    if ((deviceCount > 0) && (IsEmulationEnabled() == false))
    {
        drmProcs.pfnDrmFreeDevices(pDevices, deviceCount);
    }

    return result;
}

// =====================================================================================================================
Result Platform::InitProperties()
{
    // Set the support flags for linux features.
    m_properties.supportNonSwapChainPresents = 0;
    m_properties.supportBlockIfFlipping      = 0;
    m_properties.explicitPresentModes        = 0;

    return Pal::Platform::InitProperties();
}
// =====================================================================================================================
// Enumerates all physical screens present in the system.
//
// This method may be called multiple times, because clients will use it to re-enumerate GPU's and screens after a
// Device lost error occurs.
Result Platform::ReQueryScreens(
    uint32*  pScreenCount,
    void*    pStorage[MaxScreens],
    IScreen* pScreens[MaxScreens])
{
    uint32 screens = 0;
    Result result = Result::Success;

    for (uint32 i = 0; i < m_deviceCount; i++)
    {
        uint32 screenCount = 0;

        result = static_cast<Device*>(m_pDevice[i])->GetScreens(&screenCount,
                                                                pStorage ? &pStorage[screens] : nullptr,
                                                                pScreens ? &pScreens[screens] : nullptr);
        if (result == Result::Success)
        {
            screens += screenCount;
        }
        else
        {
            break;
        }
    }

    if (result == Result::Success)
    {
        *pScreenCount = screens;
    }

    return result;
}

// =====================================================================================================================
const DrmLoader& Platform::GetDrmLoader()
{
    Result result = Result::Success;
    if (m_drmLoader.Initialized() == false)
    {
        result = m_drmLoader.Init(this);
#if defined(PAL_DEBUG_PRINTS)
        if (result == Result::Success)
        {
            m_drmLoader.SetLogPath(m_logPath);
        }
#endif
    }
    // if drmLoader cannot be initialized, it is a fatal error that we cannot recover from that gracefully.
    // Some dependency must not be satisfied, and the following calls to functions from external libraries
    // will segfault directly.
    PAL_ASSERT(result == Result::Success);

    return m_drmLoader;
}

#if PAL_HAVE_DRI3_PLATFORM
// =====================================================================================================================
const Dri3Loader& Platform::GetDri3Loader()
{
    Result result = Result::Success;
    if (m_dri3Loader.Initialized() == false)
    {
        result = m_dri3Loader.Init(nullptr);
#if defined(PAL_DEBUG_PRINTS)
        if (result == Result::Success)
        {
            m_dri3Loader.SetLogPath(m_logPath);
        }
#endif
    }
    // if dri3Loader cannot be initialized, it is a fatal error that we cannot recover from that gracefully.
    // Some dependency must not be satisfied, and the following calls to functions from external libraries
    // will segfault directly.
    PAL_ASSERT(result == Result::Success);

    return m_dri3Loader;
}
#endif

#if PAL_HAVE_WAYLAND_PLATFORM
// =====================================================================================================================
const WaylandLoader& Platform::GetWaylandLoader()
{
    Result result = Result::Success;

    if (m_waylandLoader.Initialized() == false)
    {
        result = m_waylandLoader.Init(nullptr);
#if defined(PAL_DEBUG_PRINTS)
        if (result == Result::Success)
        {
            m_waylandLoader.SetLogPath(m_logPath);
        }
#endif
    }
    // if waylandLoader cannot be initialized, it is a fatal error that we cannot recover from that gracefully.
    // Some dependency must not be satisfied, and the following calls to functions from external libraries
    // will segfault directly.
    PAL_ASSERT(result == Result::Success);

    return m_waylandLoader;
}
#endif

// =====================================================================================================================

// =====================================================================================================================
// Helper function to translate a AMDGPU_PIXEL_FORMAT enumeration into a PAL format.
SwizzledFormat AmdgpuFormatToPalFormat(
    AMDGPU_PIXEL_FORMAT format,
    bool*               pFormatChange,      // [out] If non-null, this will indicate if the image associated with this
                                            //       format must opt in for view format change.
    bool*               pDepthStencilUsage) // [out] If non-null, this will indicate if the image associated with this
                                            //       format must enable depth stencil usage.
{
    struct FormatInfo
    {
        SwizzledFormat format;
        bool           formatChange;
        bool           depthStencilUsage;
    };

    // Define a table for all DXGI based enums with PAL equivalents. Typeless formats are aliased to a valid
    // format within the same "format family".

    constexpr uint32 FirstAmdgpuBasedFormat = AMDGPU_PIXEL_FORMAT__8;
    constexpr uint32 LastAmdgpuBasedFormat  = AMDGPU_PIXEL_FORMAT__32_32_32_32_FLOAT;
    constexpr uint32 NumAmdgpuBasedFormats  = LastAmdgpuBasedFormat - FirstAmdgpuBasedFormat + 1;

    const FormatInfo AmdgpuBasedFormatTable[NumAmdgpuBasedFormats] =
    {
        // AMDGPU_PIXEL_FORMAT__8 = 1
        { { ChNumFormat::X8_Unorm,
            { ChannelSwizzle::X,    ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__4_4 = 2
        { { ChNumFormat::X4Y4_Unorm,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Zero,  ChannelSwizzle::One }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__3_3_2 = 3
        { { ChNumFormat::Undefined,
            { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero,  ChannelSwizzle::One }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__RESERVED_4 = 4
        { { ChNumFormat::Undefined,
            { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero,  ChannelSwizzle::One }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__16 = 5
        { { ChNumFormat::X16_Unorm,
            { ChannelSwizzle::X,    ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__16_FLOAT = 6
        { { ChNumFormat::X16_Float,
            { ChannelSwizzle::X,    ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__8_8 = 7
        { { ChNumFormat::X8Y8_Unorm,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__5_6_5 = 8
        { { ChNumFormat::X5Y6Z5_Unorm,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Z,    ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__6_5_5 = 9
        { { ChNumFormat::Undefined,
            { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero,  ChannelSwizzle::One }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__1_5_5_5 = 10
        { { ChNumFormat::X1Y5Z5W5_Unorm,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Z,    ChannelSwizzle::W    }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__4_4_4_4 = 11
        { { ChNumFormat::X4Y4Z4W4_Unorm,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Z,    ChannelSwizzle::W    }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__5_5_5_1 = 12
        { { ChNumFormat::X5Y5Z5W1_Unorm,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Z,    ChannelSwizzle::W    }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__32 = 13
        { { ChNumFormat::X32_Uint,
            { ChannelSwizzle::X,    ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__32_FLOAT = 14
        { { ChNumFormat::X32_Float,
            { ChannelSwizzle::X,    ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__16_16 = 15
        { { ChNumFormat::X16Y16_Unorm,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__16_16_FLOAT = 16
        { { ChNumFormat::X16Y16_Float,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__8_24 = 17
        { { ChNumFormat::Undefined,
            { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__8_24_FLOAT = 18
        { { ChNumFormat::Undefined,
            { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__24_8 = 19
        { { ChNumFormat::Undefined,
            { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__24_8_FLOAT = 20
        { { ChNumFormat::Undefined,
            { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false
        },
        // AMDGPU_PIXEL_FORMAT__10_11_11 = 21
        { { ChNumFormat::Undefined,
            { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false
        },
        // AMDGPU_PIXEL_FORMAT__10_11_11_FLOAT = 22
        { { ChNumFormat::X10Y11Z11_Float,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Z,    ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__11_11_10 = 23
        { { ChNumFormat::Undefined,
            { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero,  ChannelSwizzle::One }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__11_11_10_FLOAT = 24
        { { ChNumFormat::X11Y11Z10_Float,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Z,    ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__2_10_10_10 = 25
        { { ChNumFormat::Undefined,
            { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero,  ChannelSwizzle::One }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__8_8_8_8 = 26
        { { ChNumFormat::X8Y8Z8W8_Unorm,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Z,    ChannelSwizzle::W    }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__10_10_10_2 = 27
        { { ChNumFormat::X10Y10Z10W2_Unorm,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Z,    ChannelSwizzle::W    }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__X24_8_32_FLOAT = 28
        { { ChNumFormat::D32_Float_S8_Uint,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
            true
        },
        // AMDGPU_PIXEL_FORMAT__32_32 = 29
        { { ChNumFormat::X32Y32_Uint,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__32_32_FLOAT = 30
        { { ChNumFormat::X32Y32_Float,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__16_16_16_16 = 31
        { { ChNumFormat::X16Y16Z16W16_Unorm,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Z,    ChannelSwizzle::W    }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__16_16_16_16_FLOAT = 32
        { { ChNumFormat::X16Y16Z16W16_Float,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Z,    ChannelSwizzle::W    }},
            false,
        },
        // AMDGPU_PIXEL_FORMAT__RESERVED_33 = 33
        { { ChNumFormat::Undefined,
            { ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One  }},
            false
        },
        // AMDGPU_PIXEL_FORMAT__32_32_32_32 = 34
        { { ChNumFormat::X32Y32Z32W32_Uint,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Z,    ChannelSwizzle::W    }},
            false
        },
        // AMDGPU_PIXEL_FORMAT__32_32_32_32_FLOAT = 35
        { { ChNumFormat::X32Y32Z32W32_Float,
            { ChannelSwizzle::X,    ChannelSwizzle::Y,    ChannelSwizzle::Z,    ChannelSwizzle::W    }},
            false,
        },
    };

    SwizzledFormat palFormat         = {};
    bool           formatChange      = false;
    bool           depthStencilUsage = false;

    // If the input format falls within one of the tables do a lookup.
    if ((format >= FirstAmdgpuBasedFormat) && (format <= LastAmdgpuBasedFormat))
    {
        palFormat         = AmdgpuBasedFormatTable[format - FirstAmdgpuBasedFormat].format;
        formatChange      = AmdgpuBasedFormatTable[format - FirstAmdgpuBasedFormat].formatChange;
        depthStencilUsage = AmdgpuBasedFormatTable[format - FirstAmdgpuBasedFormat].depthStencilUsage;
    }
    else
    {
        palFormat = UndefinedSwizzledFormat;
    }

    PAL_ASSERT(Formats::IsUndefined(palFormat.format) == false);

    if (pFormatChange != nullptr)
    {
        *pFormatChange = formatChange;
    }

    if (pDepthStencilUsage != nullptr)
    {
        *pDepthStencilUsage = depthStencilUsage;
    }

    return palFormat;
}

// =====================================================================================================================
uint32 AmdGpuToAddrTileModeConversion(
    AMDGPU_TILE_MODE tileMode)
{
    uint32 addrTileMode = ADDR_TM_LINEAR_GENERAL;
    switch (tileMode)
    {
    case AMDGPU_TILE_MODE__LINEAR_GENERAL:
        addrTileMode = ADDR_TM_LINEAR_GENERAL;
        break;
    case AMDGPU_TILE_MODE__LINEAR_ALIGNED:
        addrTileMode = ADDR_TM_LINEAR_ALIGNED;
        break;
    case AMDGPU_TILE_MODE__1D_TILED_THIN1:
        addrTileMode = ADDR_TM_1D_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__1D_TILED_THICK:
        addrTileMode = ADDR_TM_1D_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__2D_TILED_THIN1:
        addrTileMode = ADDR_TM_2D_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__2D_TILED_THIN2:
        addrTileMode = ADDR_TM_2D_TILED_THIN2;
        break;
    case AMDGPU_TILE_MODE__2D_TILED_THIN4:
        addrTileMode = ADDR_TM_2D_TILED_THIN4;
        break;
    case AMDGPU_TILE_MODE__2D_TILED_THICK:
        addrTileMode = ADDR_TM_2D_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__2B_TILED_THIN1:
        addrTileMode = ADDR_TM_2B_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__2B_TILED_THIN2:
        addrTileMode = ADDR_TM_2B_TILED_THIN2;
        break;
    case AMDGPU_TILE_MODE__2B_TILED_THIN4:
        addrTileMode = ADDR_TM_2B_TILED_THIN4;
        break;
    case AMDGPU_TILE_MODE__2B_TILED_THICK:
        addrTileMode = ADDR_TM_2B_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__3D_TILED_THIN1:
        addrTileMode = ADDR_TM_3D_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__3D_TILED_THICK:
        addrTileMode = ADDR_TM_3D_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__3B_TILED_THIN1:
        addrTileMode = ADDR_TM_3B_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__3B_TILED_THICK:
        addrTileMode = ADDR_TM_3B_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__2D_TILED_XTHICK:
        addrTileMode = ADDR_TM_2D_TILED_XTHICK;
        break;
    case AMDGPU_TILE_MODE__3D_TILED_XTHICK:
        addrTileMode = ADDR_TM_3D_TILED_XTHICK;
        break;
    case AMDGPU_TILE_MODE__PRT_TILED_THIN1:
        addrTileMode = ADDR_TM_PRT_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__PRT_2D_TILED_THIN1:
        addrTileMode = ADDR_TM_PRT_2D_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__PRT_3D_TILED_THIN1:
        addrTileMode = ADDR_TM_PRT_3D_TILED_THIN1;
        break;
    case AMDGPU_TILE_MODE__PRT_TILED_THICK:
        addrTileMode = ADDR_TM_PRT_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__PRT_2D_TILED_THICK:
        addrTileMode = ADDR_TM_PRT_2D_TILED_THICK;
        break;
    case AMDGPU_TILE_MODE__PRT_3D_TILED_THICK:
        addrTileMode = ADDR_TM_PRT_3D_TILED_THICK;
        break;
    default:
        addrTileMode = ADDR_TM_LINEAR_GENERAL;
        break;
    }

    return addrTileMode;
}

// =====================================================================================================================
// convert the tiling mode from PAL definition to AMDGPU definition.
AMDGPU_TILE_MODE AddrToAmdGpuTileModeConversion(
    uint32 tileMode)
{
    constexpr AMDGPU_TILE_MODE AmdGpuTileModes[] =
    {
        AMDGPU_TILE_MODE__LINEAR_GENERAL,       //ADDR_TM_LINEAR_GENERAL      = 0,
        AMDGPU_TILE_MODE__LINEAR_ALIGNED,       //ADDR_TM_LINEAR_ALIGNED      = 1,
        AMDGPU_TILE_MODE__1D_TILED_THIN1,       //ADDR_TM_1D_TILED_THIN1      = 2,
        AMDGPU_TILE_MODE__1D_TILED_THICK,       //ADDR_TM_1D_TILED_THICK      = 3,
        AMDGPU_TILE_MODE__2D_TILED_THIN1,       //ADDR_TM_2D_TILED_THIN1      = 4,
        AMDGPU_TILE_MODE__2D_TILED_THIN2,       //ADDR_TM_2D_TILED_THIN2      = 5,
        AMDGPU_TILE_MODE__2D_TILED_THIN4,       //ADDR_TM_2D_TILED_THIN4      = 6,
        AMDGPU_TILE_MODE__2D_TILED_THICK,       //ADDR_TM_2D_TILED_THICK      = 7,
        AMDGPU_TILE_MODE__2B_TILED_THIN1,       //ADDR_TM_2B_TILED_THIN1      = 8,
        AMDGPU_TILE_MODE__2B_TILED_THIN2,       //ADDR_TM_2B_TILED_THIN2      = 9,
        AMDGPU_TILE_MODE__2B_TILED_THIN4,       //ADDR_TM_2B_TILED_THIN4      = 10,
        AMDGPU_TILE_MODE__2B_TILED_THICK,       //ADDR_TM_2B_TILED_THICK      = 11,
        AMDGPU_TILE_MODE__3D_TILED_THIN1,       //ADDR_TM_3D_TILED_THIN1      = 12,
        AMDGPU_TILE_MODE__3D_TILED_THICK,       //ADDR_TM_3D_TILED_THICK      = 13,
        AMDGPU_TILE_MODE__3B_TILED_THIN1,       //ADDR_TM_3B_TILED_THIN1      = 14,
        AMDGPU_TILE_MODE__3B_TILED_THICK,       //ADDR_TM_3B_TILED_THICK      = 15,
        AMDGPU_TILE_MODE__2D_TILED_XTHICK,      //ADDR_TM_2D_TILED_XTHICK     = 16,
        AMDGPU_TILE_MODE__3D_TILED_XTHICK,      //ADDR_TM_3D_TILED_XTHICK     = 17,
        AMDGPU_TILE_MODE__INVALID,              //ADDR_TM_POWER_SAVE          = 18,
        AMDGPU_TILE_MODE__PRT_TILED_THIN1,      //ADDR_TM_PRT_TILED_THIN1     = 19,
        AMDGPU_TILE_MODE__PRT_2D_TILED_THIN1,   //ADDR_TM_PRT_2D_TILED_THIN1  = 20,
        AMDGPU_TILE_MODE__PRT_3D_TILED_THIN1,   //ADDR_TM_PRT_3D_TILED_THIN1  = 21,
        AMDGPU_TILE_MODE__PRT_TILED_THICK,      //ADDR_TM_PRT_TILED_THICK     = 22,
        AMDGPU_TILE_MODE__PRT_2D_TILED_THICK,   //ADDR_TM_PRT_2D_TILED_THICK  = 23,
        AMDGPU_TILE_MODE__PRT_3D_TILED_THICK,   //ADDR_TM_PRT_3D_TILED_THICK  = 24,
        AMDGPU_TILE_MODE__INVALID,              //ADDR_TM_COUNT               = 25,
    };
    return AmdGpuTileModes[tileMode];
}

// =====================================================================================================================
// convert the surface format from PAL definition to AMDGPU definition.
AMDGPU_PIXEL_FORMAT PalToAmdGpuFormatConversion(
    SwizzledFormat format)
{
    // we don't support types of format other than R8G8B8A8 or B8G8R8A8 so far.
    return AMDGPU_PIXEL_FORMAT__8_8_8_8;
}

static_assert(static_cast<uint32>(ADDR_DISPLAYABLE) ==
              static_cast<uint32>(AMDGPU_MICRO_TILE_MODE__DISPLAYABLE),
              "mismatched tile type enum value");
static_assert(static_cast<uint32>(ADDR_NON_DISPLAYABLE) ==
              static_cast<uint32>(AMDGPU_MICRO_TILE_MODE__NON_DISPLAYABLE),
              "mismatched tile type enum value");
static_assert(static_cast<uint32>(ADDR_DEPTH_SAMPLE_ORDER) ==
              static_cast<uint32>(AMDGPU_MICRO_TILE_MODE__DEPTH_SAMPLE_ORDER),
              "mismatched tile type enum value");
static_assert(static_cast<uint32>(ADDR_ROTATED) ==
              static_cast<uint32>(AMDGPU_MICRO_TILE_MODE__ROTATED),
              "mismatched tile type enum value");
static_assert(static_cast<uint32>(ADDR_THICK) ==
              static_cast<uint32>(AMDGPU_MICRO_TILE_MODE__THICK),
              "mismatched tile type enum value");
static_assert(static_cast<uint32>(ADDR_SW_LINEAR)         == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_LINEAR)        ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_256B_S)         == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_256B_S)        ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_256B_D)         == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_256B_D)        ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_256B_R)         == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_256B_R)        ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_4KB_Z)          == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_4KB_Z)         ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_4KB_S)          == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_4KB_S)         ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_4KB_D)          == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_4KB_D)         ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_4KB_R)          == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_4KB_R)         ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_Z)         == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_64KB_Z)        ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_S)         == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_64KB_S)        ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_D)         == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_64KB_D)        ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_R)         == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_64KB_R)        ,
              "mismatched swizzle enum value");
// Make following assertions use ADDR_SW_RESERVED[0/1/2/3] after the definitions imported from SW AddrLib
static_assert(static_cast<uint32>(ADDR_SW_64KB_R + 1)     == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_VAR_Z)         ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_R + 2)     == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_VAR_S)         ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_R + 3)     == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_VAR_D)         ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_R + 4)     == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_VAR_R)         ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_Z_T)       == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_64KB_Z_T)      ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_S_T)       == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_64KB_S_T)      ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_D_T)       == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_64KB_D_T)      ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_R_T)       == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_64KB_R_T)      ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_4KB_Z_X)        == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_4KB_Z_X)       ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_4KB_S_X)        == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_4KB_S_X)       ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_4KB_D_X)        == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_4KB_D_X)       ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_4KB_R_X)        == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_4KB_R_X)       ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_Z_X)       == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_64KB_Z_X)      ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_S_X)       == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_64KB_S_X)      ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_D_X)       == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_64KB_D_X)      ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_64KB_R_X)       == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_64KB_R_X)      ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_VAR_Z_X)        == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_VAR_Z_X)       ,
              "mismatched swizzle enum value");
// Make following assertions use ADDR_SW_RESERVED[4/5] after the definitions imported from SW AddrLib
static_assert(static_cast<uint32>(ADDR_SW_VAR_Z_X + 1)    == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_VAR_S_X)       ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_VAR_Z_X + 2)    == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_VAR_D_X)       ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_VAR_R_X)        == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_VAR_R_X)       ,
              "mismatched swizzle enum value");
static_assert(static_cast<uint32>(ADDR_SW_LINEAR_GENERAL) == static_cast<uint32>(AMDGPU_SWIZZLE_MODE_LINEAR_GENERAL),
              "mismatched swizzle enum value");

static_assert(static_cast<uint32>(ImageType::Tex1d) == static_cast<uint32>(AMDGPU_ADDR_RSRC_TEX_1D),
              "mismatched image type enum value");
static_assert(static_cast<uint32>(ImageType::Tex2d) == static_cast<uint32>(AMDGPU_ADDR_RSRC_TEX_2D),
              "mismatched image type enum value");
static_assert(static_cast<uint32>(ImageType::Tex3d) == static_cast<uint32>(AMDGPU_ADDR_RSRC_TEX_3D),
              "mismatched image type enum value");
static_assert(static_cast<uint32>(ImageType::Count) == static_cast<uint32>(AMDGPU_ADDR_RSRC_MAX_TYPE),
              "mismatched image type enum value");
} // Amdgpu
} // Pal
