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

//=============================================================================
// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief System info writer implementation
//=============================================================================

#include "system_info_writer.h"

#include <array>
#include <fstream>
#include <vector>

#include "json.hpp"

#include <ddAmdGpuInfo.h>
#include <ddVersion.h>

#include "definitions.h"
#include "system_info_reader.h"

namespace
{

    bool IsClosedSourceDriver(const std::string& driver_name)
    {
        bool result = false;
#if   defined(__linux__)
        // TODO(mguerret): Determine if any other driver names can be classified as closed source
        result = (driver_name == "vulkan-amdgpu-pro") || (driver_name == "vulkan-amdgpu");
#else
        DD_UNUSED(driver_name);
#endif
        return result;
    }

#ifdef __linux__
    std::string ProcessCommand(const std::string& command, const std::string& output_file, bool strip = true)
    {
        std::string result;

        const int system_result = system(command.c_str());
        if (system_result == 0)
        {
            std::ifstream file_stream;
            file_stream.open(output_file);
            if (file_stream.is_open())
            {
                result = std::string((std::istreambuf_iterator<char>(file_stream)), std::istreambuf_iterator<char>());
                if (strip)
                {
                    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
                }
                file_stream.close();
                remove(output_file.c_str());
            }
        }

        return result;
    }
#endif

    std::string QueryDriverName()
    {
        std::string result{};

#ifdef WIN32
        // No real AMD defined name for Windows driver, so we define our own.
        result = "AMD Windows";
#else
        // Determine which Linux os we are running
        DevDriver::Platform::OsInfo os_info{};
        DevDriver::Platform::QueryOsInfo(&os_info);

        std::vector<std::string> commands;
        if (DevDriver::Platform::Strcmpi(os_info.name, "Arch Linux") == 0)
        {
            // TODO(mguerret): Arch Linux driver name
        }
        else if (DevDriver::Platform::Strcmpi(os_info.name, "Fedora Linux") == 0)
        {
            commands.emplace_back(
                R"(dnf info installed amdvlk | awk 'BEGIN{FS="Name"}; gsub(":", "", $2);' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/name.txt)");
        }
        else
        {
            // Support Ubuntu pro driver name
            commands.emplace_back(R"(dpkg-query -s vulkan-amdgpu-pro 2>/dev/null | awk 'BEGIN{FS="Package: "}{print $2}' | awk 'NF > 0' > /tmp/name.txt)");

            // Support Ubuntu closed amdvlk driver name
            commands.emplace_back(R"(dpkg-query -s vulkan-amdgpu 2>/dev/null | awk 'BEGIN{FS="Package: "}{print $2}' | awk 'NF > 0' > /tmp/name.txt)");

            // Support Ubuntu GitHub amdvlk driver name
            commands.emplace_back(R"(dpkg-query -s amdvlk 2>/dev/null | awk 'BEGIN{FS="Package: "}{print $2}' | awk 'NF > 0' > /tmp/name.txt)");

            // Support Ubuntu mesa vulkan graphics driver name
            commands.emplace_back(R"(dpkg-query -s mesa-vulkan-drivers 2>/dev/null | awk 'BEGIN{FS="Package: "}{print $2}' | awk 'NF > 0' > /tmp/name.txt)");
        }

        for (const auto& command : commands)
        {
            const int system_result = system(command.c_str());
            if (system_result == 0)
            {
                const char*   name_file_name = "/tmp/name.txt";
                std::ifstream name_file_stream;
                name_file_stream.open(name_file_name);
                if (name_file_stream.is_open())
                {
                    std::string line_str;
                    getline(name_file_stream, line_str);
                    name_file_stream.close();
                    remove(name_file_name);
                    if (!line_str.empty())
                    {
                        result = line_str;
                        break;
                    }
                }
            }
        }
#endif

        return result;
    }

    std::string QueryDriverDescription()
    {
        std::string result{};
#ifdef WIN32
        // No real driver description on Windows, so we define our own
        result = "AMD Windows Driver";
#elif defined(__linux__)
        // Determine which Linux os we are running
        DevDriver::Platform::OsInfo os_info{};
        DevDriver::Platform::QueryOsInfo(&os_info);

        std::vector<std::string> commands;
        if (DevDriver::Platform::Strcmpi(os_info.name, "Arch Linux") == 0)
        {
            // TODO(mguerret): Arch Linux driver description
        }
        else if (DevDriver::Platform::Strcmpi(os_info.name, "Fedora Linux") == 0)
        {
            commands.emplace_back(
                R"(dnf info installed amdvlk | awk 'BEGIN{FS="Description"}; gsub(":", "", $2);' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/description.txt)");
        }
        else
        {
            // Support Ubuntu pro driver description
            commands.emplace_back(
                R"(dpkg-query -s vulkan-amdgpu-pro 2>/dev/null | awk 'BEGIN{FS="Description: "}{print $2}' | awk 'NF > 0' > /tmp/description.txt)");

            // Support Ubuntu AMDVLK driver description
            commands.emplace_back(
                R"(dpkg-query -s vulkan-amdgpu 2>/dev/null | awk 'BEGIN{FS="Description: "}{print $2}' | awk 'NF > 0' > /tmp/description.txt)");

            // Support Ubuntu AMDVLK driver description
            commands.emplace_back(R"(dpkg-query -s amdvlk 2>/dev/null | awk 'BEGIN{FS="Description: "}{print $2}' | awk 'NF > 0' > /tmp/description.txt)");

            // Support mesa vulkan graphics driver
            commands.emplace_back(
                R"(dpkg-query -s mesa-vulkan-drivers 2>/dev/null | awk 'BEGIN{FS="Description: "}{print $2}' | awk 'NF > 0' > /tmp/description.txt)");
        }

        for (const auto& command : commands)
        {
            const int system_result = system(command.c_str());
            if (system_result == 0)
            {
                // Read the description file that was just written.
                const char*   description_file_name = "/tmp/description.txt";
                std::ifstream description_file_stream;
                description_file_stream.open(description_file_name);
                if (description_file_stream.is_open())
                {
                    // Read the description string from the file.
                    std::string line_str;
                    getline(description_file_stream, line_str);
                    // Close the file stream and remove the file.
                    description_file_stream.close();
                    remove(description_file_name);
                    if (!line_str.empty())
                    {
                        result = line_str;
                        break;
                    }
                }
            }
        }
#endif
        return result;
    }

    std::string QueryDriverPackagingVersion()
    {
        std::string result{};
#ifdef WIN32
        result = QueryRegistryString("SOFTWARE\\ATI Technologies\\Install", "ReleaseVersion");
#elif defined(__linux__)
        // Determine which Linux os we are running
        DevDriver::Platform::OsInfo os_info{};
        DevDriver::Platform::QueryOsInfo(&os_info);

        std::vector<std::string> commands;
        if (DevDriver::Platform::Strcmpi(os_info.name, "Arch Linux") == 0)
        {
            // Support Arch Linux driver version using pacman
            // NOTE: This specifically looks for the vulkan-amdgpu-pro package which should be fine
            // considering our tools specifically target Vulkan on Linux.
            commands.emplace_back(
                R"(pacman -Q --info vulkan-amdgpu-pro | awk '/Version/ { gsub("_",".", $3); gsub("-", ".", $3); print $3 }' > /tmp/version.txt)");
        }
        else if (DevDriver::Platform::Strcmpi(os_info.name, "Fedora Linux") == 0)
        {
            // Support Fedora Linux (Fedora 35+)
            // NOTE: This looks specifically for AMDVLK driver which is the AMD open source
            // vulkan driver. The is currently no official support for amdgpu-pro on Fedora
            commands.emplace_back("dnf info installed amdvlk | awk '/Version/ {print $3}' > /tmp/version.txt");
        }
        else
        {
            // Support Ubuntu pro driver version
            commands.emplace_back(R"(dpkg-query -s vulkan-amdgpu-pro 2>/dev/null | awk 'BEGIN{FS="Version: "}{print $2}' | awk 'NF > 0' > /tmp/version.txt)");

            // Support Ubuntu AMDVLK driver version
            commands.emplace_back(R"(dpkg-query -s vulkan-amdgpu 2>/dev/null | awk 'BEGIN{FS="Version: "}{print $2}' | awk 'NF > 0' > /tmp/version.txt)");

            // Support Ubuntu AMDVLK driver version
            commands.emplace_back(R"(dpkg-query -s amdvlk 2>/dev/null | awk 'BEGIN{FS="Version: "}{print $2}' | awk 'NF > 0' > /tmp/version.txt)");

            // Support Ubuntu mesa driver version
            commands.emplace_back(R"(dpkg-query -s mesa-vulkan-drivers 2>/dev/null | awk 'BEGIN{FS="Version: "}{print $2}' | awk 'NF > 0' > /tmp/version.txt)");
        }

        for (const auto& command : commands)
        {
            const int system_result = system(command.c_str());
            if (system_result == 0)
            {
                const char*   version_file_name = "/tmp/version.txt";
                std::ifstream version_file_stream;
                version_file_stream.open(version_file_name);
                if (version_file_stream.is_open())
                {
                    std::string line_str;
                    getline(version_file_stream, line_str);
                    version_file_stream.close();
                    remove(version_file_name);
                    if (!line_str.empty())
                    {
                        result = line_str;
                        break;
                    }
                }
            }
        }
#endif
        return result;
    }

    std::string QueryDriverSoftwareVersion()
    {
        std::string result{};
#ifdef WIN32
        result = QueryRegistryString("SOFTWARE\\ATI Technologies\\Install", "RadeonSoftwareVersion");
#elif defined(__linux__)
        // No implementation for Linux
#endif
        return result;
    }

    void WritePlatformConfig(DevDriver::IStructuredWriter* writer)
    {
#ifdef __linux__
        writer->KeyAndBeginMap(kNodeStringLinux);
        {
            bool        power_dpm_writable = false;
            struct stat st
            {
            };
            const char* clocks_file = "/sys/class/drm/card0/device/power_dpm_force_performance_level";
            if (stat(clocks_file, &st) == 0)
            {
                mode_t permissions = st.st_mode;
                if ((permissions & S_IWUSR) != 0 && (permissions & S_IWGRP) != 0 && (permissions & S_IWOTH) != 0)
                {
                    power_dpm_writable = true;
                }
            }
            writer->KeyAndValue(kNodeStringPowerDpmWritable, power_dpm_writable);

            // Query libdrm version
            DevDriver::Vector<DevDriver::AmdGpuInfo> gpus(DevDriver::Platform::GenericAllocCb);
            DD_UNHANDLED_RESULT(QueryGpuInfo(DevDriver::Platform::GenericAllocCb, &gpus));
            DD_ASSERT(!gpus.IsEmpty());

            // TODO(mguerret): libdrm version ideally should not be GPU specific
            const DevDriver::AmdGpuInfo& gpu = gpus[0];
            writer->KeyAndBeginMap(kNodeStringDrm);
            {
                writer->KeyAndValue(kNodeStringMajor, gpu.drmVersion.Major);
                writer->KeyAndValue(kNodeStringMinor, gpu.drmVersion.Minor);
            }
            writer->EndMap();
        }
        writer->EndMap();
#else
        DD_UNUSED(writer);
#endif
    }

    void WriteSingleGpu(DevDriver::IStructuredWriter* writer, const DevDriver::AmdGpuInfo& gpu)
    {
        DD_ASSERT(writer != nullptr);

        writer->BeginMap();
        {
            writer->KeyAndValue(kNodeStringName, gpu.name);

            // PCI info
            writer->KeyAndBeginMap(kNodeStringPci);
            {
                writer->KeyAndValue(kNodeStringPciBus, gpu.pci.bus);
                writer->KeyAndValue(kNodeStringDevice, gpu.pci.device);
                writer->KeyAndValue(kNodeStringPciFunction, gpu.pci.function);
            }
            writer->EndMap();

#ifdef __linux__
            // libdrm information
            writer->KeyAndBeginMap(kNodeStringDrm);
            {
                writer->KeyAndValue(kNodeStringMajor, gpu.drmVersion.Major);
                writer->KeyAndValue(kNodeStringMinor, gpu.drmVersion.Minor);
            }
            writer->EndMap();
#endif

            // ASIC info
            writer->KeyAndBeginMap(kNodeStringAsic);
            {
                writer->KeyAndValue(kNodeStringAsicGpuIndex, gpu.asic.gpuIndex);
                writer->KeyAndValue(kNodeStringAsicGpuCounterFrequency, gpu.asic.gpuCounterFreq);

                writer->KeyAndBeginMap(kNodeStringAsicEngineClockSpeed);
                {
                    writer->KeyAndValue(kNodeStringMin, gpu.engineClocks.min);
                    writer->KeyAndValue(kNodeStringMax, gpu.engineClocks.max);
                }
                writer->EndMap();

                writer->KeyAndBeginMap(kNodeStringAsicIds);
                {
                    writer->KeyAndValue(kNodeStringAsicGfxEngine, gpu.asic.ids.gfxEngineId);
                    writer->KeyAndValue(kNodeStringAsicFamily, gpu.asic.ids.family);
                    writer->KeyAndValue(kNodeStringAsicERev, gpu.asic.ids.eRevId);
                    writer->KeyAndValue(kNodeStringAsicRevision, gpu.asic.ids.revisionId);
                    writer->KeyAndValue(kNodeStringDevice, gpu.asic.ids.deviceId);
                }
                writer->EndMap();
            }
            writer->EndMap();

            // Memory info
            writer->KeyAndBeginMap(kNodeStringMemory);
            {
                writer->KeyAndValueEnumOrHex(kNodeStringType, gpu.memory.type);

                writer->KeyAndValue(kNodeStringMemoryOpsPerClock, gpu.memory.memOpsPerClock);
                writer->KeyAndValue(kNodeStringMemoryBusBitWidth, gpu.memory.busBitWidth);
                writer->KeyAndValue(kNodeStringMemoryBandwith, gpu.memory.BandwidthInBytes());

                writer->KeyAndBeginMap(kNodeStringMemoryClockSpeed);
                {
                    writer->KeyAndValue(kNodeStringMin, gpu.memory.clocksHz.min);
                    writer->KeyAndValue(kNodeStringMax, gpu.memory.clocksHz.max);
                }
                writer->EndMap();

                writer->KeyAndBeginMap(kNodeStringHeaps);
                {
                    writer->KeyAndBeginMap(kNodeStringLocal);
                    {
                        writer->KeyAndValue(kNodeStringPhysicalAddress, gpu.memory.localHeap.physAddr);
                        writer->KeyAndValue(kNodeStringSize, gpu.memory.localHeap.size);
                    }
                    writer->EndMap();

                    writer->KeyAndBeginMap(kNodeStringInvisible);
                    {
                        writer->KeyAndValue(kNodeStringPhysicalAddress, gpu.memory.invisibleHeap.physAddr);
                        writer->KeyAndValue(kNodeStringSize, gpu.memory.invisibleHeap.size);
                    }
                    writer->EndMap();
                }
                writer->EndMap();

                // A non-zero value indicated that this memory type is supported
                if (gpu.memory.hbccSize != 0)
                {
                    writer->KeyAndValue(kNodeStringHbccSize, gpu.memory.hbccSize);
                }

                writer->KeyAndBeginList(kNodeStringExcludedVaRanges);
                {
                    for (size_t i = 0; i < DevDriver::AmdGpuInfo::kMaxExcludedVaRanges; i++)
                    {
                        const auto& range = gpu.memory.excludedVaRanges[i];
                        if (range.size == 0)
                        {
                            continue;
                        }

                        writer->BeginMap();
                        {
                            writer->KeyAndValue(kNodeStringBase, range.base);
                            writer->KeyAndValue(kNodeStringSize, range.size);
                        }
                        writer->EndMap();
                    }
                }
                writer->EndList();
            }
            writer->EndMap();

            writer->KeyAndBeginMap(kNodeStringBigSw);
            {
                writer->KeyAndValue(kNodeStringMajor, gpu.bigSwVersion.Major);
                writer->KeyAndValue(kNodeStringMinor, gpu.bigSwVersion.Minor);
                writer->KeyAndValue(kNodeStringMisc, gpu.bigSwVersion.Misc);
            }
            writer->EndMap();
        }
        writer->EndMap();
    }

#ifdef __linux__
    void ParseLinuxCpuInfoJson(DevDriver::IStructuredWriter* writer, const std::string& json)
    {
        nlohmann::json structure;
        structure = nlohmann::json::parse(json);

        auto object_array = structure.front();
        DD_ASSERT(object_array.is_array());

        const std::string field_key = "field";
        const std::string data_key  = "data";

        std::string architecture{};
        std::string cpu_name{};
        std::string vendor_id{};
        uint32_t    socket_count{};
        uint32_t    core_count{};
        uint64_t    min_speed{};
        uint64_t    max_speed{};
        uint32_t    logical_core_count{};
        for (auto& object : object_array)
        {
            std::string field_name = object[field_key].get<std::string>();
            if (field_name == "Architecture:")
            {
                architecture = object[data_key].get<std::string>();
            }
            if (field_name == "Model name:")
            {
                cpu_name = object[data_key].get<std::string>();
            }
            if (field_name == "Vendor ID:")
            {
                vendor_id = object[data_key].get<std::string>();
            }
            if (field_name == "Socket(s):")
            {
                socket_count = std::atoi(object[data_key].get<std::string>().c_str());
            }
            if (field_name == "Core(s) per socket:")
            {
                core_count = std::atoi(object[data_key].get<std::string>().c_str());
            }
            if (field_name == "CPU min MHz:")
            {
                min_speed = std::atoi(object[data_key].get<std::string>().c_str());
            }
            if (field_name == "CPU max MHz:")
            {
                max_speed = std::atoi(object[data_key].get<std::string>().c_str());
            }
            if (field_name == "CPU(s):")
            {
                logical_core_count = std::atoi(object[data_key].get<std::string>().c_str());
            }
        }
        uint32_t physical_core_count = socket_count * core_count;

        writer->BeginMap();
        {
            writer->KeyAndValue(kNodeStringArchitecture, architecture.c_str());
            writer->KeyAndValue(kNodeStringName, cpu_name.c_str());
            writer->KeyAndValue(kNodeStringCpuVendorId, vendor_id.c_str());
            writer->KeyAndValue(kNodeStringCpuPhysicalCoreCount, physical_core_count);
            writer->KeyAndValue(kNodeStringCpuLogicalCoreCount, logical_core_count);
            writer->KeyAndBeginMap(kNodeStringSpeed);
            {
                writer->KeyAndValue(kNodeStringMin, min_speed);
                writer->KeyAndValue(kNodeStringMax, max_speed);
            }
            writer->EndMap();
            writer->KeyAndValue(kNodeStringCpuId, "");
            writer->KeyAndValue(kNodeStringCpuDeviceId, "");
        }
        writer->EndMap();
    }

    void ParseLinuxCpuInfoAwk(DevDriver::IStructuredWriter* writer)
    {
        writer->BeginMap();
        {
            // Access CPU architecture
            const char* arch_query = R"(lscpu | awk 'BEGIN{FS="Architecture:"}{ print $2}' | awk 'NF > 0' | awk '{gsub(/\n/, ""); print $1}' > /tmp/arch.txt)";
            const std::string arch = ProcessCommand(arch_query, "/tmp/arch.txt");
            writer->KeyAndValue(kNodeStringArchitecture, arch.c_str());

            // Access CPU name
            const char* name_query =
                R"(lscpu | awk 'BEGIN{FS="Model name:"}{ print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/name.txt)";
            const std::string name = ProcessCommand(name_query, "/tmp/name.txt");
            writer->KeyAndValue(kNodeStringName, name.c_str());

            // Access CPU vendor ID
            const char* vendor_query =
                R"(lscpu | awk 'BEGIN{FS="Vendor ID:"}{ print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/vendor.txt)";
            const std::string vendor = ProcessCommand(vendor_query, "/tmp/vendor.txt");
            writer->KeyAndValue(kNodeStringCpuVendorId, vendor.c_str());

            // Access physical core count
            const char* core_count_query =
                R"(lscpu | awk 'BEGIN{FS="Core\\(s\\) per socket:"}{print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/cores.txt)";
            const char* socket_count_query =
                R"(lscpu | awk 'BEGIN{FS="Socket\\(s\\):"}{print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/sockets.txt)";
            const uint32_t socket_count        = std::atoi(ProcessCommand(socket_count_query, "/tmp/sockets.txt").c_str());
            const uint32_t core_count          = std::atoi(ProcessCommand(core_count_query, "/tmp/cores.txt").c_str());
            const uint32_t physical_core_count = socket_count * core_count;
            writer->KeyAndValue(kNodeStringCpuPhysicalCoreCount, physical_core_count);

            // Access logical core count
            const char* logical_core_count_command =
                R"(lscpu | awk 'BEGIN{FS="CPU\\(s\\):"}{print $2}' | awk 'NF > 0' | awk '{print $1}' | head -1 > /tmp/logical_cores.txt)";
            const uint32_t logical_core_count = std::atoi(ProcessCommand(logical_core_count_command, "/tmp/logical_cores.txt").c_str());
            writer->KeyAndValue(kNodeStringCpuLogicalCoreCount, logical_core_count);

            writer->KeyAndBeginMap(kNodeStringSpeed);
            {
                // Access min speed
                const char* min_speed_query =
                    R"(lscpu | awk 'BEGIN{FS="CPU min MHz:"}{print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/min_speed.txt)";
                const uint64_t min_speed = std::atoi(ProcessCommand(min_speed_query, "/tmp/min_speed.txt").c_str());
                writer->KeyAndValue(kNodeStringMin, min_speed);

                // Access max speed
                const char* speed_query =
                    R"(lscpu | awk 'BEGIN{FS="CPU max MHz:"}{print $2}' | awk 'NF > 0' | awk '{sub(/^[ \t]+/, ""); print $0}' > /tmp/speed.txt)";
                const uint64_t max_speed = std::atoi(ProcessCommand(speed_query, "/tmp/speed.txt").c_str());
                writer->KeyAndValue(kNodeStringMax, max_speed);
            }
            writer->EndMap();
        }
        writer->EndMap();
    }

    void QueryLinuxCpuInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);

        // Access CPU information using lscpu --json
        const char* query_json    = R"(lscpu --json > /tmp/cpuinfo.json)";
        std::string file_contents = ProcessCommand(query_json, "/tmp/cpuinfo.json", false);
        if (file_contents.empty())
        {
            ParseLinuxCpuInfoJson(writer, file_contents);
        }
        else
        {
            // If JSON representation is unavailable, fallback to using
            // direct lscpu commands with awk to parse output.
            ParseLinuxCpuInfoAwk(writer);
        }
    }
#endif
}  // namespace

namespace system_info_utils
{
    void SystemInfoWriter::WriteDevDriverInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);

        writer->KeyAndBeginMap(kNodeStringDevDriver);
        {
            writer->KeyAndBeginMap(kNodeStringVersion);
            {
                writer->KeyAndValue(kNodeStringMajor, GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION);
            }
            writer->EndMap();

            writer->KeyAndValue(kNodeStringTag, DevDriver::GetVersionString());
        }
        writer->EndMap();
    }

    void SystemInfoWriter::WriteSystemInfo(DevDriver::IStructuredWriter* writer)
    {
        // Write version information
        writer->KeyAndValue(kNodeStringVersion, kVersion);

        // Developer driver information
        WriteDevDriverInfo(writer);

        // Operating System information
        WriteOsInfo(writer);

        // Driver information
        WriteDriverInfo(writer);

        // CPU information
        WriteCpuInfo(writer);

        // GPU information
        WriteGpuInfo(writer);
    }

    void SystemInfoWriter::WriteDriverInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);

        writer->KeyAndBeginMap(kNodeStringDriver);
        {
            const std::string name = QueryDriverName();
            // Driver name
            writer->KeyAndValue(kNodeStringName, name.c_str());

            // Is closed source driver
            writer->KeyAndValue(kNodeStringIsClosedSource, IsClosedSourceDriver(name));

            // Driver description
            writer->KeyAndValue(kNodeStringDescription, QueryDriverDescription().c_str());

            // Driver packaging version
            writer->KeyAndValue(kNodeStringDriverPackagingVersion, QueryDriverPackagingVersion().c_str());

            // Driver software version
            writer->KeyAndValue(kNodeStringDriverSoftwareVersion, QueryDriverSoftwareVersion().c_str());
        }
        writer->EndMap();
    }

    void SystemInfoWriter::WriteOsInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);

        DevDriver::Platform::OsInfo os_info{};
        DD_UNHANDLED_RESULT(DevDriver::Platform::QueryOsInfo(&os_info));

        writer->KeyAndBeginMap(kNodeStringOs);
        {
            writer->KeyAndValue(kNodeStringType, os_info.type);
            writer->KeyAndValue(kNodeStringName, os_info.name);
            writer->KeyAndValue(kNodeStringDescription, os_info.description);
            writer->KeyAndValue(kNodeStringHostName, os_info.hostname);
            writer->KeyAndBeginMap(kNodeStringMemory);
            {
                writer->KeyAndValue(kNodeStringMemoryPhysical, os_info.physMemory);
                writer->KeyAndValue(kNodeStringMemorySwap, os_info.swapMemory);
            }
            writer->EndMap();

            // Write platform specific configuration
            writer->KeyAndBeginMap(kNodeStringConfig);
            {
                WritePlatformConfig(writer);
            }
            writer->EndMap();
        }
        writer->EndMap();
    }

    void SystemInfoWriter::WriteCpuInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);

        writer->KeyAndBeginList(kNodeStringCpus);
        {
#if   defined(__linux__)
            // TODO(mguerret): Support multiple CPU on Linux
            QueryLinuxCpuInfo(writer);
#else
            // no-op on other platforms
#endif
        }
        writer->EndList();
    }

    void SystemInfoWriter::WriteGpuInfo(DevDriver::IStructuredWriter* writer)
    {
        DD_ASSERT(writer != nullptr);

        writer->KeyAndBeginList(kNodeStringGpus);
        {
            DevDriver::Vector<DevDriver::AmdGpuInfo> gpus(DevDriver::Platform::GenericAllocCb);
            DD_UNHANDLED_RESULT(QueryGpuInfo(DevDriver::Platform::GenericAllocCb, &gpus));

            for (const DevDriver::AmdGpuInfo& gpu : gpus)
            {
                WriteSingleGpu(writer, gpu);
            }
        }
        writer->EndList();
    }

#ifdef SYSTEM_INFO_ENABLE_RDF
    DD_RESULT SystemInfoWriter::WriteRdfChunk(rdfChunkFileWriter* file_writer, const std::string& json)
    {
        DD_ASSERT(file_writer != nullptr);

        // Ensure we are writing only the system info chunk data
        std::string chunk_data = SystemInfoReader::Parse(json);
        const auto  chunk_size = static_cast<int64_t>(chunk_data.size());

        // Write chunk to file
        rdfChunkCreateInfo create_info{};
        create_info.version    = kSystemInfoChunkVersion;
        create_info.headerSize = 0;
        create_info.pHeader    = nullptr;
        memcpy(create_info.identifier, kSystemInfoChunkIdentifier, strlen(kSystemInfoChunkIdentifier));

        int index{};
        int rdf_result = rdfChunkFileWriterWriteChunk(file_writer, &create_info, chunk_size, chunk_data.data(), &index);
        if (rdf_result != rdfResultOk)
        {
            return DD_RESULT_DD_GENERIC_UNKNOWN;
        }

        return DD_RESULT_SUCCESS;
    };
#endif

}  // namespace system_info_utils
