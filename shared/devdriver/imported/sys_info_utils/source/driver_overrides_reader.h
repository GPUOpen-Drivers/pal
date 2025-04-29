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
/// @brief Driver Overrides reader definition
///
/// The Driver Overrides Reader class is responsible for parsing the Driver Overrides JSON data.
/// Settings and Experiments are filtered to include only those that the user has modified.
//=============================================================================

#ifndef SYSTEM_INFO_UTILS_SOURCE_DRIVER_OVERRIDES_READER_H_
#define SYSTEM_INFO_UTILS_SOURCE_DRIVER_OVERRIDES_READER_H_

#include <string>

#ifdef DRIVER_OVERRIDES_ENABLE_RDF
#include <amdrdf.h>
#endif

namespace driver_overrides_utils
{
    /// @brief Parses Driver Overrides RDF chunk.
    class DriverOverridesReader
    {
    public:
        /// @brief Default constructor
        DriverOverridesReader() = delete;

        /// @brief delete copy constructor
        DriverOverridesReader(const DriverOverridesReader&) = delete;

        /// @brief delete move constructor
        DriverOverridesReader(DriverOverridesReader&&) = delete;

        /// @brief Default destructor
        ~DriverOverridesReader() = delete;

        /// @brief delete assignment operator
        DriverOverridesReader& operator=(const DriverOverridesReader&) = delete;

        /// @brief delete move assignment operator
        DriverOverridesReader& operator=(DriverOverridesReader&&) = delete;

        /// @brief Parses the Driver Overrides JSON representation.
        /// @param [in] driver_overrides_json_string The Driver Overrides chunk JSON string.
        /// @param [in] version The version of the Driver Overrides chunk.
        /// @param [in, out] out_processed_json_text The processed JSON string for the Driver Overrides tree.
        /// @return true if successfully parsed, false otherwise
        static bool Parse(const std::string& out_processed_json_text, std::uint32_t version, std::string& out_processed_json_string);

#ifdef DRIVER_OVERRIDES_ENABLE_RDF
#ifdef RDF_CXX_BINDINGS
        /// @brief Parses driver Overrides chunk from RDF file.
        /// @param [in] file The RDF file
        /// @return true if the chunk is present, false otherwise.
        static bool IsChunkPresent(rdf::ChunkFile& file);

        /// @brief Parses driver Overrides chunk from RDF file.
        /// @param [in] file The RDF file
        /// @param [in, out] out_processed_json_text The processed JSON string for the Driver Overrides tree.
        /// @return true on successful parse, false otherwise
        static bool Parse(rdf::ChunkFile& file, std::string& out_processed_json_text);
#endif
        /// @brief Parses driver Overrides chunk from RDF file.
        /// @param [in] file The RDF file
        /// @return true if the chunk is present, false otherwise.
        static bool IsChunkPresent(rdfChunkFile* file);

        /// @brief Parses driver Overrides chunk from RDF file.
        /// @param [in] file The RDF file
        /// @param [in, out] out_processed_json_text The processed JSON string for the Driver Overrides tree.
        /// @return true on successful parse, false otherwise
        static bool Parse(rdfChunkFile* file, std::string& out_processed_json_text);
#endif
    };
}  // namespace driver_overrides_utils

#endif
