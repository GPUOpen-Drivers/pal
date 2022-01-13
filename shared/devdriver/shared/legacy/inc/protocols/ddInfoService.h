/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "ddUriInterface.h"
#include "util/hashMap.h"
#include "util/string.h"

namespace DevDriver
{
namespace InfoURIService
{

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  1.0    | Initial version                                                                                          |
***********************************************************************************************************************
*/

// =====================================================================================================================
// Info Service
// Used to query general information remotely in the form of structured JSON strings.
class InfoService : public IService
{
public:
    // The "WriteFunc" callback type used to write a registered info source.
    typedef void(*WriteFunc)(IStructuredWriter* pWriter, void* pUserdata);

    // Info service constants.
    DD_STATIC_CONST Version kInfoServiceVersion = 1;
    DD_STATIC_CONST uint32 kMaxInfoSourceNameStrLen = 64;

    // Structure used to hold info source registration parameters.
    struct InfoSource
    {
        FixedString<kMaxInfoSourceNameStrLen>   name;               ///< The name of the info source.
        Version                                 version;            ///< The version of the info source.
        WriteFunc                               pfnWriteCallback;   ///< The callback used to write the info response.
        void*                                   pUserdata;          ///< An optional user data pointer.
    };

public:
    explicit InfoService(const AllocCb& allocCb);
    virtual ~InfoService();

    // Returns the service name.
    const char* GetName() const override final;

    // Returns the service version.
    Version GetVersion() const override final { return kInfoServiceVersion; }

    // Handles a request from a developer driver client.
    Result HandleRequest(IURIRequestContext* pContext) override final;

    // Overview:
    //      Register an info source with the service.
    //      The user must provide a structure containing parameters for the
    //      info source including the name, version, and write callback function.
    //      An optional userdata pointer can be provided to be accessed within the info writing callback.
    //
    // Return Values:
    //      Result::Success - If the info source name and callback are valid, and a source
    //                        with the same name has not been previously registered.
    //                        Otherwise, an error result code is returned.
    Result RegisterInfoSource(const InfoSource& sourceInfo);

    // Unregisters an info source from the service using the provided source name.
    void UnregisterInfoSource(const char* pName);

    // Clears all currently registered info sources from the service.
    void ClearInfoSources();

private:
    // Overview:
    //      Retrieve a JSON string containing info for all registered info sources.
    //      The callback function registered with each info source is responsible for
    //      writing the "value" map of the info response.
    //
    // Request Format:
    //      uri request:
    //          info://all
    //      uri arguments:
    //          None
    //      POST data:
    //          None
    // Response Data Format: JSON
    //
    //  {
    //      "<source name>": {
    //          "version":"<source version>",
    //          "value":{
    //            <info written by registered pfnWriteCallback>
    //          }
    //      },
    //      ...
    //  }
    //
    // Return Values:
    //      Result::Success - if all registered info sources were written successfully.
    //      Otherwise, an error result code is returned.
    Result HandleGetAllInfoSources(IURIRequestContext* pContext);

    // Overview:
    //      Retrieve a list of names of all registered info sources.
    //      The response doesn't include any data besides the list of registered info source names.
    //
    // Request Format:
    //      uri request:
    //          info://list
    //      uri arguments:
    //          None
    //      POST data:
    //          None
    // Response Data Format: JSON
    //
    //        [
    //            "<source name 1>",
    //            ...
    //            "<source name n>"
    //        ]
    //
    // Return Values:
    //      Result::Success - if the list of all registered info source names was written successfully.
    //      Otherwise, an error result code is returned.
    Result HandleGetInfoSourceList(IURIRequestContext* pContext);

    // Overview:
    //      Retrieve a single info source using the source name.
    //      The response includes the source version, and the value
    //      written by the registered pfnWriteCallback.
    //
    // Request Format:
    //      uri request:
    //          info://getInfo <sourceName>
    //      uri arguments:
    //          sourceName - A string containing the requested info source name.
    //      POST data:
    //          None
    // Response Data Format: JSON
    //
    //    {
    //        "version":<source version>,
    //        "value":{
    //            <info written by registered pfnWriteCallback>
    //        }
    //    }
    //
    // Return Values:
    //      Result::Success - If the info source name was previously registered and the info was written successfully.
    //      Otherwise, an error result code is returned.
    Result HandleGetInfoSourceByName(IURIRequestContext* pContext, const char* pName);

    // Write a response for the given info source using the provided writer.
    void WriteInfoSource(const InfoSource& source, IStructuredWriter* pWriter);

    // Mutex to protect access to the registered info sources map which can be accessed asynchronously
    // from separate threads from the URI calls (RegisterInfoSource vs. URI calls).
    Platform::Mutex m_infoSourceMutex;

    // A hash map of all the info sources that are currently available to the InfoService.
    HashMap<DevDriver::FixedString<kMaxInfoSourceNameStrLen>, InfoSource, 16> m_registeredInfoSources;
};
} // InfoURIService
} // DevDriver
