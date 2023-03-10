/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
/// @brief System info reader implementation
//=============================================================================

#include "system_info_reader.h"

#include "json.hpp"

#include "definitions.h"

namespace
{
    /// @brief Gets the property from the JSON object with a default object if that key is not present.
    /// @tparam [in] T The type to convert the specified JSON property into.
    /// @param [in] parent The object to get the property from.
    /// @param [in] name The name of the property to get from the parent object.
    /// @param [in] fallback The default value to use if the value is not present.
    /// @return The value from the JSON object or the default value.
    template <typename T>
    T Get(const nlohmann::json& parent, char const* name, const T& fallback)
    {
        if (parent.contains(name))
        {
            return parent[name];
        }

        return fallback;
    }

    /// @brief Check if a node with the given name exists as a child of the provided parent node.
    ///
    /// @param [in] parent The parent JSON node whose children will be searched.
    /// @param [in] name The name of the child node to search for.
    /// @return True when a child JSON node with the given name exists, and false if it doesn't.
    bool DoesNodeExist(const nlohmann::json& parent, const std::string& name)
    {
        bool result = false;

        auto node_iter = parent.find(name);
        if (node_iter != parent.end())
        {
            result = true;
        }

        return result;
    }

}  // namespace

namespace system_info_utils
{
    /// @brief The V1 implementation handles parsing Version 1 of the system info JSON.
    class SystemInfoParserV1
    {
    public:
        /// @brief Constructor.
        SystemInfoParserV1() = default;

        /// @brief Destructor.
        virtual ~SystemInfoParserV1() = default;

        /// @brief Process the System Info value JSON node.
        ///
        /// @param [in] system_value_node The parent JSON node containing system info fields.
        /// @param [in, out] system_info The structure to be populated with the parsed system info members.
        virtual void ProcessSystemValueNode(const nlohmann::json& system_value_node, SystemInfo& system_info)
        {
            if (DoesNodeExist(system_value_node, kNodeStringDevDriver))
            {
                ProcessDevDriverNode(system_value_node[kNodeStringDevDriver], system_info.devdriver);
            }

            if (DoesNodeExist(system_value_node, kNodeStringDriver))
            {
                ProcessDriverNode(system_value_node[kNodeStringDriver], system_info.driver);
            }

            if (DoesNodeExist(system_value_node, kNodeStringOs))
            {
                ProcessOsNode(system_value_node[kNodeStringOs], system_info.os);
            }

            if (DoesNodeExist(system_value_node, kNodeStringCpus))
            {
                ProcessCpusNode(system_value_node[kNodeStringCpus], system_info.cpus);
            }

            if (DoesNodeExist(system_value_node, kNodeStringGpus))
            {
                ProcessGpusNodes(system_value_node[kNodeStringGpus], system_info.gpus);
            }
        }

    protected:
        /// @brief Process the DevDriver info JSON node.
        ///
        /// @param [in] dev_driver_root The parent JSON node for the DevDriver section.
        /// @param [in, out] dev_driver_info The structure to be populated with the parsed DevDriver info.
        virtual void ProcessDevDriverNode(const nlohmann::json& dev_driver_root, DevDriverInfo& dev_driver_info)
        {
            if (DoesNodeExist(dev_driver_root, kNodeStringVersion))
            {
                const auto& version_root = dev_driver_root[kNodeStringVersion];

                dev_driver_info.major_version = Get<uint32_t>(version_root, kNodeStringMajor, 0);
            }
            dev_driver_info.tag = Get<std::string>(dev_driver_root, kNodeStringTag, "");
        }

        /// @brief Process the OS Memory info JSON node.
        ///
        /// @param [in] memory_root The parent JSON node for the memory section.
        /// @param [in, out] memory_info The structure to be populated with the parsed system memory info.
        virtual void ProcessOsMemoryNode(const nlohmann::json& memory_root, OsMemoryInfo& memory_info)
        {
            memory_info.physical = Get<uint64_t>(memory_root, kNodeStringMemoryPhysical, 0);
            memory_info.swap     = Get<uint64_t>(memory_root, kNodeStringMemorySwap, 0);
        }

        /// @brief Process the Event Tracing for Windows info JSON node.
        ///
        /// @param [in] etw_root The parent JSON node for the ETW Info section.
        /// @param [in, out] etw_info The structure to be populated with the parsed ETW info.
        virtual void ProcessEtwNode(const nlohmann::json& etw_root, EtwSupportInfo& etw_info)
        {
            etw_info.is_supported   = Get<bool>(etw_root, kNodeStringSupported, false);
            etw_info.has_permission = Get<bool>(etw_root, kNodeStringHasPermission, false);
            etw_info.status_code    = Get<unsigned long>(etw_root, kNodeStringStatusCode, 0);
        }

        /// @brief Process the Operating System info JSON node.
        ///
        /// @param [in] os_root The parent JSON node for the operating system section.
        /// @param [in, out] os_info The structure to be populated with the parsed operating system info.
        virtual void ProcessOsNode(const nlohmann::json& os_root, OsInfo& os_info)
        {
            os_info.name     = Get<std::string>(os_root, kNodeStringName, "");
            os_info.desc     = Get<std::string>(os_root, kNodeStringDescription, "");
            os_info.hostname = Get<std::string>(os_root, kNodeStringHostName, "");

            if (DoesNodeExist(os_root, kNodeStringMemory))
            {
                ProcessOsMemoryNode(os_root[kNodeStringMemory], os_info.memory);
            }

            if (DoesNodeExist(os_root, kNodeStringConfig))
            {
                const auto& config_root = os_root[kNodeStringConfig];

                if (DoesNodeExist(config_root, kNodeStringLinux))
                {
                    const auto& linux_root = config_root[kNodeStringLinux];

                    os_info.config.power_dpm_writable = Get<bool>(linux_root, kNodeStringPowerDpmWritable, false);

                    if (DoesNodeExist(linux_root, kNodeStringDrm))
                    {
                        const auto& drm_root             = linux_root[kNodeStringDrm];
                        os_info.config.drm_major_version = Get<uint32_t>(drm_root, kNodeStringMajor, 0);
                        os_info.config.drm_minor_version = Get<uint32_t>(drm_root, kNodeStringMinor, 0);
                    }
                }

                if (DoesNodeExist(config_root, kNodeStringWindows))
                {
                    const auto& windows_root = config_root[kNodeStringWindows];
                    if (DoesNodeExist(windows_root, kNodeStringEtwSupport))
                    {
                        ProcessEtwNode(windows_root[kNodeStringEtwSupport], os_info.config.etw_support_info);
                    }
                }
            }
        }

        /// @brief Process the CPU info JSON node.
        ///
        /// @param [in] cpu_root The parent JSON node for the CPU section.
        /// @param [in, out] cpu_info The structure to be populated with the parsed CPU info.
        virtual void ProcessCpusNode(const nlohmann::json& cpus_root, std::vector<CpuInfo>& cpus)
        {
            // Iterate over each node under the GPUs parent.
            auto first = cpus_root.begin();
            auto last  = cpus_root.end();
            for (auto iter = first; iter != last; ++iter)
            {
                const nlohmann::json& cpu_node = *iter;

                CpuInfo cpu = {};

                cpu.name               = Get<std::string>(cpu_node, kNodeStringName, "");
                cpu.architecture       = Get<std::string>(cpu_node, kNodeStringArchitecture, "");
                cpu.cpu_id             = Get<std::string>(cpu_node, kNodeStringCpuId, "");
                cpu.device_id          = Get<std::string>(cpu_node, kNodeStringCpuDeviceId, "");
                cpu.vendor_id          = Get<std::string>(cpu_node, kNodeStringCpuVendorId, "");
                cpu.num_logical_cores  = Get<uint32_t>(cpu_node, kNodeStringCpuLogicalCoreCount, 0);
                cpu.num_physical_cores = Get<uint32_t>(cpu_node, kNodeStringCpuPhysicalCoreCount, 0);

                if (DoesNodeExist(cpu_node, kNodeStringVirtualization))
                {
                    cpu.virtualization = Get<std::string>(cpu_node, kNodeStringVirtualization, "");
                }

                if (DoesNodeExist(cpu_node, kNodeStringSpeed))
                {
                    // Speed object
                    const nlohmann::json& cpu_speed_node = cpu_node[kNodeStringSpeed];
                    cpu.max_clock_speed                  = Get<uint32_t>(cpu_speed_node, kNodeStringMax, 0);
                }

                cpus.push_back(cpu);
            }
        }

        /// @brief Process the GPU PCI info JSON node.
        ///
        /// @param [in] pci_root The parent JSON node for the PCI info section.
        /// @param [in, out] pci_info The structure to be populated with the parsed PCI info.
        virtual void ProcessGpuPciNode(const nlohmann::json& pci_root, PciInfo& pci_info)
        {
            pci_info.bus      = Get<uint32_t>(pci_root, kNodeStringPciBus, 0);
            pci_info.device   = Get<uint32_t>(pci_root, kNodeStringDevice, 0);
            pci_info.function = Get<uint32_t>(pci_root, kNodeStringPciFunction, 0);
        }

        /// @brief Process the clock frequency info JSON node.
        ///
        /// @param [in] clock_hz_root The parent JSON node containing the clock Hz info.
        /// @param [in, out] clock_info The structure to be populated with the parsed clock frequency info.
        virtual void ProcessClockInfoNode(const nlohmann::json& clock_hz_root, ClockInfo& clock_info)
        {
            clock_info.min = Get<uint64_t>(clock_hz_root, kNodeStringMin, 0);
            clock_info.max = Get<uint64_t>(clock_hz_root, kNodeStringMax, 0);
        }

        /// @brief Process the ASIC Id info JSON node.
        ///
        /// @param [in] asic_id_info_root The parent JSON node containing the ASIC Id info.
        /// @param [in, out] asic_id_info The structure to be populated with the parsed ASIC Id info.
        virtual void ProcessAsicIdInfoNode(const nlohmann::json& asic_id_info_root, IdInfo& asic_id_info)
        {
            asic_id_info.gfx_engine = Get<uint32_t>(asic_id_info_root, kNodeStringAsicGfxEngine, 0);
            asic_id_info.family     = Get<uint32_t>(asic_id_info_root, kNodeStringAsicFamily, 0);
            asic_id_info.e_rev      = Get<uint32_t>(asic_id_info_root, kNodeStringAsicERev, 0);
            asic_id_info.revision   = Get<uint32_t>(asic_id_info_root, kNodeStringAsicRevision, 0);
            asic_id_info.device     = Get<uint32_t>(asic_id_info_root, kNodeStringDevice, 0);
        }

        /// @brief Process an individual GPU's ASIC info JSON node.
        ///
        /// @param [in] asic_root The parent JSON node containing the GPU's ASIC info.
        /// @param [in, out] asic_info The structure to be populated with the parsed ASIC info.
        virtual void ProcessGpuAsicNode(const nlohmann::json& asic_root, AsicInfo& asic_info)
        {
            asic_info.gpu_index        = Get<uint32_t>(asic_root, kNodeStringAsicGpuIndex, static_cast<uint32_t>(-1));
            asic_info.gpu_counter_freq = Get<uint32_t>(asic_root, kNodeStringAsicGpuCounterFrequency, 0);

            if (DoesNodeExist(asic_root, kNodeStringAsicEngineClockSpeed))
            {
                ProcessClockInfoNode(asic_root[kNodeStringAsicEngineClockSpeed], asic_info.engine_clock_hz);
            }

            if (DoesNodeExist(asic_root, kNodeStringAsicIds))
            {
                ProcessAsicIdInfoNode(asic_root[kNodeStringAsicIds], asic_info.id_info);
            }
        }

        /// @brief Process a GPU device's memory heap info JSON node.
        ///
        /// @param [in] heaps_root The parent JSON node containing the device's memory heaps info.
        /// @param [in, out] heaps The vector of structures to be populated with the parsed memory heap info.
        virtual void ProcessGpuMemoryHeapsNode(const nlohmann::json& heaps_root, std::vector<HeapInfo>& heaps)
        {
            auto first_heap_iter = heaps_root.begin();
            auto last_heap_iter  = heaps_root.end();
            for (auto heap_iter = first_heap_iter; heap_iter != last_heap_iter; ++heap_iter)
            {
                const nlohmann::json& current_heap_node = *heap_iter;

                HeapInfo current_heap  = {};
                current_heap.heap_type = heap_iter.key();
                current_heap.phys_addr = Get<uint64_t>(current_heap_node, kNodeStringPhysicalAddress, 0);
                current_heap.size      = Get<uint64_t>(current_heap_node, kNodeStringSize, 0);

                heaps.push_back(current_heap);
            }
        }

        /// @brief Process a device's excluded memory region info JSON node.
        ///
        /// @param [in] excluded_ranges_root The parent JSON node containing the device's excluded memory range info.
        /// @param [in, out] excluded_ranges The vector of structures to be populated with the parsed excluded memory range info.
        virtual void ProcessExcludedVaRanges(const nlohmann::json& excluded_ranges_root, std::vector<ExcludedRangeInfo>& excluded_ranges)
        {
            // Iterate over each node under the GPUs parent.
            auto first_excluded_range_iter = excluded_ranges_root.begin();
            auto last_excluded_range_iter  = excluded_ranges_root.end();
            for (auto range_iter = first_excluded_range_iter; range_iter != last_excluded_range_iter; ++range_iter)
            {
                const nlohmann::json& excluded_range_root = *range_iter;

                ExcludedRangeInfo current_range = {};
                current_range.base              = Get<uint64_t>(excluded_range_root, kNodeStringBase, 0);
                current_range.size              = Get<uint64_t>(excluded_range_root, kNodeStringSize, 0);

                excluded_ranges.push_back(current_range);
            }
        }

        /// @brief Process a device's memory info JSON node.
        ///
        /// @param [in] memory_root The parent JSON node containing the device's memory info.
        /// @param [in, out] memory_info The structure to be populated with the parsed memory info.
        virtual void ProcessGpuMemoryNode(const nlohmann::json& memory_root, MemoryInfo& memory_info)
        {
            memory_info.type              = Get<std::string>(memory_root, kNodeStringType, "");
            memory_info.mem_ops_per_clock = Get<uint32_t>(memory_root, kNodeStringMemoryOpsPerClock, 0);
            memory_info.bus_bit_width     = Get<uint32_t>(memory_root, kNodeStringMemoryBusBitWidth, 0);
            memory_info.bandwidth         = Get<uint64_t>(memory_root, kNodeStringMemoryBandwith, 0);

            if (DoesNodeExist(memory_root, kNodeStringMemoryClockSpeed))
            {
                ProcessClockInfoNode(memory_root[kNodeStringMemoryClockSpeed], memory_info.mem_clock_hz);
            }

            if (DoesNodeExist(memory_root, kNodeStringHeaps))
            {
                ProcessGpuMemoryHeapsNode(memory_root[kNodeStringHeaps], memory_info.heaps);
            }

            if (DoesNodeExist(memory_root, kNodeStringExcludedVaRanges))
            {
                ProcessExcludedVaRanges(memory_root[kNodeStringExcludedVaRanges], memory_info.excluded_va_ranges);
            }
        }

        /// @brief Process a software version info JSON node.
        ///
        /// @param [in] sw_version_root The parent JSON node containing the software version info.
        /// @param [in, out] version The structure to be populated with the parsed version info.
        virtual void ProcessSoftwareVersionNode(const nlohmann::json& sw_version_root, SoftwareVersion& version)
        {
            version.major = Get<uint32_t>(sw_version_root, kNodeStringMajor, 0);
            version.minor = Get<uint32_t>(sw_version_root, kNodeStringMinor, 0);
            version.misc  = Get<uint32_t>(sw_version_root, kNodeStringMisc, 0);
        }

        /// @brief Process the parent GPUs JSON node. There may be 1 or more devices to process.
        ///
        /// @param [in] gpus_root The parent JSON node with info for all connected GPUs.
        /// @param [in, out] gpus The vector of structures to be populated with the parsed GpuInfo fields.
        virtual void ProcessGpusNodes(const nlohmann::json& gpus_root, std::vector<GpuInfo>& gpus)
        {
            // Iterate over each node under the GPUs parent.
            auto first = gpus_root.begin();
            auto last  = gpus_root.end();
            for (auto iter = first; iter != last; ++iter)
            {
                const nlohmann::json& gpu_node = *iter;

                // Populate this GPU structure with info from the current GPU node.
                GpuInfo gpu = {};

                gpu.name = Get<std::string>(gpu_node, kNodeStringName, "");

                if (DoesNodeExist(gpu_node, kNodeStringPci))
                {
                    ProcessGpuPciNode(gpu_node[kNodeStringPci], gpu.pci);
                }

                if (DoesNodeExist(gpu_node, kNodeStringAsic))
                {
                    ProcessGpuAsicNode(gpu_node[kNodeStringAsic], gpu.asic);
                }

                if (DoesNodeExist(gpu_node, kNodeStringMemory))
                {
                    ProcessGpuMemoryNode(gpu_node[kNodeStringMemory], gpu.memory);
                }

                if (DoesNodeExist(gpu_node, kNodeStringBigSw))
                {
                    ProcessSoftwareVersionNode(gpu_node[kNodeStringBigSw], gpu.big_sw);
                }

                gpus.push_back(gpu);
            }
        }

        virtual void ProcessDriverNode(const nlohmann::json& driver_node, DriverInfo& driver)
        {
            driver.name              = Get<std::string>(driver_node, kNodeStringName, "");
            driver.description       = Get<std::string>(driver_node, kNodeStringDescription, "");
            driver.software_version  = Get<std::string>(driver_node, kNodeStringDriverSoftwareVersion, "");
            driver.packaging_version = Get<std::string>(driver_node, kNodeStringDriverPackagingVersion, "");
            driver.is_closed_source  = Get<bool>(driver_node, kNodeStringIsClosedSource, false);

            // Parse major and minor
            if (!driver.packaging_version.empty())
            {
                size_t first = driver.packaging_version.find_first_of('.');
                if (first == std::string::npos)
                {
                    return;
                }

                driver.packaging_version_major = std::atoi(driver.packaging_version.substr(0, first).c_str());

                size_t rest = first + 1;
                size_t next = driver.packaging_version.find_first_not_of("0123456789", rest);
                if (next == std::string::npos)
                {
                    return;
                }

                driver.packaging_version_minor = std::atoi(driver.packaging_version.substr(rest, next - rest).c_str());
            }
        }
    };

    /// @brief Create a parser to parse a versioned chunk of System Info JSON.
    /// @param [in] version_number The version number of the parser instance to create.
    /// @return A system info JSON parser.
    static std::shared_ptr<SystemInfoParserV1> CreateSystemInfoParser(const uint32_t version_number)
    {
        std::shared_ptr<SystemInfoParserV1> result = nullptr;

        switch (version_number)
        {
        case 1:
            result = std::make_shared<SystemInfoParserV1>();
            break;
        default:
            return nullptr;
            break;
        }

        return result;
    }

    /// @brief Process the System Info JSON node. Includes parsing the structure revision number.
    ///
    /// @param [in] system_node The parent JSON node containing system info value fields.
    /// @param [in, out] system_info The structure to be populated with the parsed system info members.
    /// @return True if parsing was successful, and false if it failed.
    static bool ProcessSystemNode(const nlohmann::json& system_node, SystemInfo& system_info)
    {
        system_info.version = Get<uint32_t>(system_node, kNodeStringVersion, 1);

        std::shared_ptr<SystemInfoParserV1> parser = CreateSystemInfoParser(system_info.version);
        assert(parser != nullptr);
        if (parser != nullptr)
        {
            parser->ProcessSystemValueNode(system_node, system_info);
            return true;
        }

        return false;
    }

    bool SystemInfoReader::Parse(const std::string& json, system_info_utils::SystemInfo& system_info)
    {
        bool result = true;

        SYSTEM_INFO_TRY
        {
            nlohmann::json structure = nlohmann::json::parse(json);

            // Process the 'system' node
            if (DoesNodeExist(structure, kNodeStringSystem))
            {
                result = ProcessSystemNode(structure[kNodeStringSystem], system_info);
            }
            else
            {
                // Process a system info only chunk of JSON. Presumably from an RDF file.
                result = ProcessSystemNode(structure, system_info);
            }
        }
        SYSTEM_INFO_CATCH(...)
        {
            // There was a failure in parsing the system info.
            result = false;
        }

        return result;
    }

    std::string SystemInfoReader::Parse(const std::string& json)
    {
        bool result = true;

        SYSTEM_INFO_TRY
        {
            nlohmann::json structure = nlohmann::json::parse(json);

            // Process the 'system' node
            if (DoesNodeExist(structure, kNodeStringSystem))
            {
                return structure[kNodeStringSystem].dump();
            }
            else
            {
                // Process a system info only chunk of JSON. Presumably from an RDF file.
                return json;
            }
        }
        SYSTEM_INFO_CATCH(...)
        {
        }

        return "";
    }

#ifdef SYSTEM_INFO_ENABLE_RDF
#ifdef RDF_CXX_BINDINGS
    bool SystemInfoReader::Parse(rdf::ChunkFile& file, SystemInfo& system_info)
    {
        bool result = false;

        if (!file.ContainsChunk(kSystemInfoChunkIdentifier))
        {
            return result;
        }

        // Access system info chunk data
        auto version = file.GetChunkVersion(kSystemInfoChunkIdentifier);
        if (version > kSystemInfoChunkVersionMax)
        {
            return result;
        }

        // Access chunk data
        auto chunk_size = file.GetChunkDataSize(kSystemInfoChunkIdentifier);

        char* buffer = new char[chunk_size + 1];
        file.ReadChunkDataToBuffer(kSystemInfoChunkIdentifier, buffer);
        buffer[chunk_size] = 0;

        result = Parse(buffer, system_info);
        delete[] buffer;

        return result;
    }
#endif
    bool SystemInfoReader::Parse(rdfChunkFile* file, SystemInfo& system_info)
    {
        assert(file != nullptr);

        bool result = false;

        int contains{};
        rdfChunkFileContainsChunk(file, kSystemInfoChunkIdentifier, 0, &contains);
        if (!contains)
        {
            return result;
        }

        // Access system info chunk data
        uint32_t version{};
        rdfChunkFileGetChunkVersion(file, kSystemInfoChunkIdentifier, 0, &version);
        if (version > kSystemInfoChunkVersionMax)
        {
            return result;
        }

        // Access chunk data
        int64_t chunk_size{};
        rdfChunkFileGetChunkDataSize(file, kSystemInfoChunkIdentifier, 0, &chunk_size);

        char* buffer = new char[chunk_size + 1];
        rdfChunkFileReadChunkData(file, kSystemInfoChunkIdentifier, 0, buffer);
        buffer[chunk_size] = 0;

        result = Parse(buffer, system_info);
        delete[] buffer;

        return result;
    }

#endif
}  // namespace system_info_utils
