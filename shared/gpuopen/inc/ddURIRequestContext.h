/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddURIRequestContext.h
* @brief A class that represents a unique URI request
***********************************************************************************************************************
*/
#pragma once
#include <ddUriInterface.h>

#include "util/ddByteWriter.h"
#include "util/ddTextWriter.h"
#include "util/ddJsonWriter.h"

namespace DevDriver
{

// A class that represents a unique URI request
class URIRequestContext : public IURIRequestContext
{
public:
    // ===== IURIRequestContext Overrides ==========
    char* GetRequestArguments() override;
    const PostDataInfo& GetPostData() const override;

    Result BeginByteResponse(IByteWriter** ppWriter) override;

    Result BeginTextResponse(ITextWriter** ppWriter) override;

    Result BeginJsonResponse(IStructuredWriter** ppWriter) override;

    // ===== Implementation  ==========

    URIRequestContext();
    virtual ~URIRequestContext() = default;

    // Reset the URIRequestContext. This can be called on an already initialized context.
    void Begin(char*                                        pArguments,
               URIDataFormat                                format,
               SharedPointer<TransferProtocol::ServerBlock> pResponseBlock,
               const PostDataInfo&                          postDataInfo);

    void End();

    URIDataFormat GetUriDataFormat() const;
    SharedPointer<TransferProtocol::ServerBlock> GetBlock() const;

private:
    static Result WriteBytes(void* pUserData, const void* pBytes, size_t numBytes);

    PostDataInfo  m_postInfo;
    char*         m_pRequestArguments;
    URIDataFormat m_responseDataFormat;

    SharedPointer<TransferProtocol::ServerBlock> m_pResponseBlock;

    // m_contextState starts as WriterSelection, transitions into one of ByteWriterSelected, TextWriterSelected,
    // or JsonWriterSelected, and then into WritingCompleted when its End() function is called
    enum class ContextState : uint32
    {
        WriterSelection,
        ByteWriterSelected,
        TextWriterSelected,
        JsonWriterSelected,
        WritingCompleted
    };

    // The internal state of selecting a writer.
    // Call `Reset()` to reset this from `WritingCompleted` to `WriterSelection`.
    ContextState m_contextState;

    // All three of these writers are initialized with the context object,
    // but only one of them should ever be in use at a time.
    // Which one is selected is chosen through the `Begin*Response()` methods.
    ByteWriter m_byteWriter;
    TextWriter m_textWriter;
    JsonWriter m_jsonWriter;
};
} // DevDriver
