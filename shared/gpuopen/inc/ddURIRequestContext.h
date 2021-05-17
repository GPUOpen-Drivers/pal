/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */
#pragma once
#include <ddUriInterface.h>

#include <util/sharedptr.h>

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

    void End(Result serviceResult);

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
