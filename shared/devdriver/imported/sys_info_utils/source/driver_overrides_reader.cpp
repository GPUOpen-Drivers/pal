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
/// @brief Driver Overrides reader implementation
//=============================================================================

#include "driver_overrides_reader.h"

#include "json.hpp"

#include "definitions.h"
#include "driver_overrides_definitions.h"

namespace driver_overrides_utils
{
    /// @brief Check if a node with the given name exists as a child of the provided parent node.
    /// @param [in] parent The parent JSON node whose children will be searched.
    /// @param [in] name The name of the child node to search for.
    /// @return True when a child JSON node with the given name exists, and false if it doesn't.
    static bool DoesNodeExist(const nlohmann::json& parent, const std::string& name)
    {
        bool result = false;

        auto node_iter = parent.find(name);
        if (node_iter != parent.end())
        {
            result = true;
        }

        return result;
    }

    /// @brief The interface for parses that process the Driver Override JSON chunk.
    class IDriverOverridesParser
    {
    public:
        /// @brief Constructor.
        IDriverOverridesParser() = default;

        /// @brief Destructor.
        virtual ~IDriverOverridesParser()
        {
        }

        /// @brief Process the Driver Overrides JSON node.
        /// @param [in] driver_overrides_json The parent JSON node containing Driver Override fields.
        /// @param [in, out] out_processed_json_text The processed JSON text.
        /// @return True if parsing was successful, false if it failed.
        virtual bool Process(const nlohmann::json& driver_overrides_json, std::string& out_processed_json_text)
        {
            SYSTEM_INFO_UNUSED(driver_overrides_json);
            SYSTEM_INFO_UNUSED(out_processed_json_text);

            return false;
        }
    };

    /// @brief JSON parser V1 for Driver Overrides.
    class DriverOverridesParserV1 : public IDriverOverridesParser
    {
    public:
        /// @brief Constructor.
        DriverOverridesParserV1() = default;

        /// @brief Destructor.
        virtual ~DriverOverridesParserV1() = default;

        /// @brief Process the Driver Overrides JSON node.
        /// The output will contain only Driver Settings/Experiments that the user has modified.
        /// @param [in] driver_overrides_json The parent JSON node containing Driver Override fields.
        /// @param [in, out] out_processed_json_text The JSON text for the filtered Driver Overrides.
        /// @return True if parsing was successful, false if it failed.
        virtual bool Process(const nlohmann::json& driver_overrides_json, std::string& out_processed_json_text)
        {
            bool           result = false;
            nlohmann::json processed_json;

            if (DoesNodeExist(driver_overrides_json, kNodeStringIsDriverExperiments))
            {
                ParseIsDriverExperiments(driver_overrides_json[kNodeStringIsDriverExperiments], processed_json);
            }
            else
            {
                is_driver_experiments_ = false;
            }

            if (DoesNodeExist(driver_overrides_json, kNodeStringComponents))
            {
                result = ParseComponents(driver_overrides_json[kNodeStringComponents], processed_json);
            }

            if (!processed_json.is_null())
            {
                out_processed_json_text = processed_json.dump();
            }
            else
            {
                out_processed_json_text.clear();
            }

            return result;
        }

    protected:
        /// @brief Parse the "IsDriverExperiments" node.
        /// @param [in] driver_overrides_json The JSON node containing the "IsDriverExperiments" field.
        /// @param [in, out] out_processed_json The processed Driver Overrides JSON data to include the "IsDriverExperiments" field.
        /// @return True if parsing was successful, false if it failed.
        bool ParseIsDriverExperiments(const nlohmann::json& driver_overrides_json, nlohmann::json& out_processed_json)
        {
            bool result = false;

            is_driver_experiments_                             = driver_overrides_json;
            out_processed_json[kNodeStringIsDriverExperiments] = is_driver_experiments_;

            return result;
        }

        /// @brief Parse the "Components" node.
        /// @param [in] driver_overrides_json The JSON node containing the "Components" array.
        /// @param [in, out] out_processed_json The processed Driver Overrides JSON data that includes filtered components.
        /// @return True if parsing was successful, false if it failed.
        bool ParseComponents(const nlohmann::json& driver_overrides_json, nlohmann::json& out_processed_json)
        {
            bool result = false;

            if (driver_overrides_json.empty())
            {
                result = true;
            }
            else
            {
                for (nlohmann::json::const_iterator components_iterator = driver_overrides_json.begin(); components_iterator != driver_overrides_json.end();
                     ++components_iterator)
                {
                    if (DoesNodeExist(components_iterator.value(), kNodeStringComponent))
                    {
                        result = ParseComponent(components_iterator.value()[kNodeStringComponent]);
                        if (!result)
                        {
                            break;
                        }

                        if (DoesNodeExist(components_iterator.value(), kNodeStringStructures))
                        {
                            result = ParseStructures(components_iterator.value()[kNodeStringStructures], out_processed_json);
                            if (!result)
                            {
                                break;
                            }
                        }
                    }
                }
            }

            return result;
        }

        /// @brief Parse the "Component" node.  The Component name will be cached for use later.
        /// @param [in] driver_overrides_json The JSON node containing the "Component" field.
        /// @return True if parsing was successful, false if it failed.
        bool ParseComponent(const nlohmann::json& driver_overrides_json)
        {
            // Store the component name.
            current_component_name_ = driver_overrides_json;

            bool result = !current_component_name_.empty();

            return result;
        }

        /// @brief Parse the "Structures" node.
        /// @param [in] driver_overrides_json The JSON node containing the "Structures" array.
        /// @param [in, out] out_processed_json The processed Driver Overrides JSON data to include filtered structures.
        /// @return True if parsing was successful, false if it failed.
        bool ParseStructures(const nlohmann::json& driver_overrides_json, nlohmann::json& out_processed_json)
        {
            bool result = false;

            for (nlohmann::json::const_iterator structures_iterator = driver_overrides_json.begin(); structures_iterator != driver_overrides_json.end();
                 ++structures_iterator)
            {
                current_structure_name_ = structures_iterator.key();
                if (current_structure_name_.empty())
                {
                    current_structure_name_ = kDriverOverridesmiscellaneousStructure;
                }

                result = ParseStructure(structures_iterator.value(), out_processed_json);
                if (!result)
                {
                    break;
                }
            }

            return result;
        }

        /// @brief Parse the "Structure" node.
        /// @param [in] driver_overrides_json The JSON node containing the "Structure" array.  The name is cached for use later.
        /// @param [in, out] out_processed_json The processed Driver Overrides JSON data to include filtered structures.
        /// @return True if parsing was successful, false if it failed.
        bool ParseStructure(const nlohmann::json& driver_overrides_json, nlohmann::json& out_processed_json)
        {
            bool result = true;
            for (nlohmann::json::const_iterator structures_iterator = driver_overrides_json.begin(); structures_iterator != driver_overrides_json.end();
                 ++structures_iterator)
            {
                result = ParseSetting(structures_iterator.value(), out_processed_json);
                if (!result)
                {
                    break;
                }
            }

            return result;
        }

        /// @brief Parse the "Setting" node.
        /// @param [in] driver_overrides_json The JSON node containing the "Setting" array.
        /// @param [in, out] out_processed_json The processed Driver Overrides JSON data to include filtered settings.
        /// @return True if parsing was successful, false if it failed.
        virtual bool ParseSetting(const nlohmann::json& driver_overrides_json, nlohmann::json& out_processed_json)
        {
            bool result = true;

            if ((DoesNodeExist(driver_overrides_json, kNodeStringSupported)) && (!driver_overrides_json[kNodeStringSupported]))
            {
                // Skip this setting if it's not supported.
                return true;
            }

            if (DoesNodeExist(driver_overrides_json, kNodeStringUserOverride) && DoesNodeExist(driver_overrides_json, kNodeStringCurrent))
            {
                if (driver_overrides_json[kNodeStringUserOverride] == driver_overrides_json[kNodeStringCurrent])
                {
                    nlohmann::json json_settings_node;
                    json_settings_node[kNodeStringValue]       = driver_overrides_json[kNodeStringUserOverride];
                    json_settings_node[kNodeStringSettingName] = driver_overrides_json[kNodeStringSettingName];
                    json_settings_node[kNodeStringDescription] = driver_overrides_json[kNodeStringDescription];

                    if (is_driver_experiments_)
                    {
                        out_processed_json[kNodeStringStructures][current_structure_name_].push_back(json_settings_node);
                    }
                    else
                    {
                        nlohmann::json json_component_node;
                        out_processed_json[kNodeStringComponents][current_component_name_][kNodeStringStructures][current_structure_name_].push_back(
                            json_settings_node);
                    }
                }
            }
            else
            {
                result = false;
            }

            return result;
        }

    protected:
        bool        is_driver_experiments_ = false;
        std::string current_component_name_;
        std::string current_structure_name_;
    };

    /// @brief Create a parser to parse a versioned chunk of Driver Overrides JSON data.
    /// @param [in] version_number The version number of the parser instance to create.
    /// @return A Driver Overrides JSON parser.
    static std::shared_ptr<IDriverOverridesParser> CreateDriverOverridesParser(const uint32_t version_number)
    {
        std::shared_ptr<IDriverOverridesParser> result = nullptr;

        switch (version_number)
        {
        case 1:
            // NOTE: Version 1 not supported.
            break;

        case 2:
        case 3:
            result = std::make_shared<DriverOverridesParserV1>();
            break;

        default:
            return nullptr;
            break;
        }

        return result;
    }

    /// @brief Process the Driver Overrides JSON node (the root node).
    /// @param [in] driver_overrides_node The parent JSON node containing Driver Overrides data.
    /// @param [in] version The version of the Driver Overrides JSON data.
    /// @param [in, out] out_processed_json_text The json string after processing the Driver Overrides data.
    /// @return True if parsing was successful, and false if it failed.
    static bool ProcessDriverOverridesNode(const nlohmann::json& driver_overrides_node, std::uint32_t version, std::string& out_processed_json_text)
    {
        bool                                    result = false;
        std::shared_ptr<IDriverOverridesParser> parser = CreateDriverOverridesParser(version);
        assert(parser != nullptr);
        if (parser != nullptr)
        {
            result = parser->Process(driver_overrides_node, out_processed_json_text);
        }

        return result;
    }

    /// @brief Implementation of the Driver Overrides Reader class.
    bool DriverOverridesReader::Parse(const std::string& driver_overrides_json_text, std::uint32_t version, std::string& out_processed_json_text)
    {
        bool result = true;
        SYSTEM_INFO_TRY
        {
            nlohmann::json driver_overrides_json = nlohmann::json::parse(driver_overrides_json_text);

            // Process a Driver Overrides chunk of JSON. Presumably from an RDF file.
            result = ProcessDriverOverridesNode(driver_overrides_json, version, out_processed_json_text);
        }
        SYSTEM_INFO_CATCH(...)
        {
            // There was a failure in parsing the Driver Overrides.
            result = false;
        }

        return result;
    }

#ifdef DRIVER_OVERRIDES_ENABLE_RDF
#ifdef RDF_CXX_BINDINGS
    bool DriverOverridesReader::IsChunkPresent(rdf::ChunkFile& file)
    {
        bool result = false;

        if (file.ContainsChunk(kDriverOverridesChunkIdentifier))
        {
            result = true;
        }

        return result;
    }

    bool DriverOverridesReader::Parse(rdf::ChunkFile& file, std::string& out_processed_json_text)
    {
        bool result = false;
        out_processed_json_text.clear();

        if (IsChunkPresent(file))
        {
            // Check if the version is supported.
            auto version = file.GetChunkVersion(kDriverOverridesChunkIdentifier);
            if ((version >= kDriverOverridesChunkVersionMin) && (version <= kDriverOverridesChunkVersionMax))
            {
                // Get the size of the chunk.
                auto chunk_size = file.GetChunkDataSize(kDriverOverridesChunkIdentifier);

                char* buffer = new (std::nothrow) char[chunk_size + 1];
                if (buffer != nullptr)
                {
                    file.ReadChunkDataToBuffer(kDriverOverridesChunkIdentifier, buffer);
                    buffer[chunk_size] = '\0';

                    // Parse the JSON text.
                    result = Parse(buffer, version, out_processed_json_text);
                    delete[] buffer;
                }
            }
        }
        else
        {
            // This chunk is optional, so no error is returned if it's not present.
            result = true;
        }

        return result;
    }
#endif
    bool DriverOverridesReader::IsChunkPresent(rdfChunkFile* file)
    {
        bool result = false;

        if (file != nullptr)
        {
            int contains{};
            rdfChunkFileContainsChunk(file, kDriverOverridesChunkIdentifier, 0, &contains);
            if (contains)
            {
                result = true;
            }
        }

        return result;
    }

    bool DriverOverridesReader::Parse(rdfChunkFile* file, std::string& out_processed_json_text)
    {
        assert(file != nullptr);

        bool result = false;
        out_processed_json_text.clear();

        if (IsChunkPresent(file))
        {
            // Check if the version is supported.
            uint32_t version{};
            rdfChunkFileGetChunkVersion(file, kDriverOverridesChunkIdentifier, 0, &version);
            if ((version >= kDriverOverridesChunkVersionMin) && (version <= kDriverOverridesChunkVersionMax))
            {
                // Get the size of the chunk.
                int64_t chunk_size{};
                rdfChunkFileGetChunkDataSize(file, kDriverOverridesChunkIdentifier, 0, &chunk_size);

                char* buffer = new (std::nothrow) char[chunk_size + 1];
                if (buffer != nullptr)
                {
                    rdfChunkFileReadChunkData(file, kDriverOverridesChunkIdentifier, 0, buffer);
                    buffer[chunk_size] = '\0';

                    // Parse the JSON text.
                    result = Parse(buffer, version, out_processed_json_text);
                    delete[] buffer;
                }
            }
        }
        else
        {
            // This chunk is optional, so no error is returned if it's not present.
            result = true;
        }

        return result;
    }

#endif
}  // namespace driver_overrides_utils
