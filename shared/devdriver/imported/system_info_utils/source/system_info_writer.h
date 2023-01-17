/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
/// @brief System info writer definition
//=============================================================================

#ifndef SYSTEM_INFO_UTILS_SOURCE_SYSTEM_INFO_WRITER_H_
#define SYSTEM_INFO_UTILS_SOURCE_SYSTEM_INFO_WRITER_H_

#include <ddApi.h>
#include <util/ddJsonWriter.h>

#ifdef SYSTEM_INFO_ENABLE_RDF
#include <amdrdf.h>
#endif

namespace system_info_utils
{
    /// @brief JSON writer for system info
    class SystemInfoWriter
    {
    public:
        /// @brief Default constructor
        SystemInfoWriter() = delete;

        /// @brief Default destructor
        ~SystemInfoWriter() = delete;

        /// @brief Writes system information JSON to structured writer
        /// @param [in] writer The structured writer
        static void WriteSystemInfo(DevDriver::IStructuredWriter* writer);

#ifdef SYSTEM_INFO_ENABLE_RDF
        /// @brief Writes system info chunk to RDF file
        /// @param [in] file_writer The RDF file writer
        /// @param [in] json The system info JSON
        /// @return DD_RESULT_SUCCESS or error code
        static DD_RESULT WriteRdfChunk(rdfChunkFileWriter* file_writer, const std::string& json);
#endif

    private:
        /// @brief Writes developer driver information to structured writer
        /// @param [in] writer The structured writer
        static void WriteDevDriverInfo(DevDriver::IStructuredWriter* writer);

        /// @brief Writes driver information to structured writer
        /// @param [in] writer The structured writer
        static void WriteDriverInfo(DevDriver::IStructuredWriter* writer);

        /// @brief Writes operating-system information to structured Writer
        /// @param [in] writer The structured writer
        static void WriteOsInfo(DevDriver::IStructuredWriter* writer);

        /// @brief Writes CPU information to structured writer
        /// @param [in] writer The structured writer
        static void WriteCpuInfo(DevDriver::IStructuredWriter* writer);

        /// @brief Writes GPU information to structured writer
        /// @param [in] writer The structured writer
        static void WriteGpuInfo(DevDriver::IStructuredWriter* writer);

        static constexpr uint32_t kVersion = 1;  ///< System Information Version
    };

}  // namespace system_info_utils

#endif
