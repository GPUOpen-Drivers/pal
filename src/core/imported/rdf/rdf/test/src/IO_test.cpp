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
#include <catch2/catch.hpp>

#include "IO.h"

#include <cstring>
#include "test_rdf.h"

TEST_CASE("rdf::MemoryStream", "[rdf]")
{
    auto ms = rdf::Stream::CreateMemoryStream();

    SECTION("Basic read/write")
    {
        static const char buffer[] = "test";

        ms.Write(sizeof(buffer), buffer);
        CHECK(ms.Tell() == 5U);
        ms.Seek(0);

        char outputBuffer[5] = {};
        ms.Read(sizeof(outputBuffer), outputBuffer);

        CHECK(::strcmp(outputBuffer, "test") == 0);
    }
}

TEST_CASE("rdf::LoadKnownGoodFile", "[rdf]")
{
    auto ms = rdf::Stream::FromReadOnlyMemory(test_rdf_len, test_rdf);

    rdf::ChunkFile cf(ms);

    CHECK(cf.ContainsChunk("chunk0"));
    CHECK(cf.ContainsChunk("chunk1"));
    CHECK(cf.ContainsChunk("chunk2"));

    cf.ReadChunkData("chunk0", 0, [](size_t size, const void* data) -> void {
        CHECK(std::string(
            static_cast<const char*>(data),
            static_cast<const char*>(data) + size) ==
            "some data");
    });

    CHECK(cf.GetChunkVersion("chunk1") == 3);
}

TEST_CASE("rdf::GetChunkVersion ignoring index bug", "[rdf]")
{
    auto ms = rdf::Stream::CreateMemoryStream();

    rdf::ChunkFileWriter writer(ms);
    writer.WriteChunk("chunk0", 0, nullptr, 0, nullptr, rdfCompressionNone, 1);
    writer.WriteChunk("chunk0", 0, nullptr, 0, nullptr, rdfCompressionNone, 2);
    writer.Close();

    ms.Seek(0);

    rdf::ChunkFile cf(ms);
    CHECK(cf.GetChunkVersion("chunk0", 0) == 1);
    CHECK(cf.GetChunkVersion("chunk0", 1) == 2);
}

TEST_CASE("rdf::ChunkFileIterator", "[rdf]")
{
    auto ms = rdf::Stream::FromReadOnlyMemory(test_rdf_len, test_rdf);

    rdf::ChunkFile cf(ms);

    int chunkCount = 0;
    auto iterator = cf.GetIterator();
    while (!iterator.IsAtEnd()) {
        ++chunkCount;
        iterator.Advance();
    }

    CHECK(chunkCount == 4);
}

TEST_CASE("rdf::ChunkFileWriter handling negative input sizes", "[rdf]")
{
    auto ms = rdf::Stream::CreateMemoryStream();
    rdf::ChunkFileWriter writer(ms);

    SECTION("Negative header size")
    {
        CHECK_THROWS_AS(writer.WriteChunk("chunk0", -1, nullptr, 0, nullptr), rdf::ApiException);
    }

    SECTION("Negative chunk size")
    {
        CHECK_THROWS_AS(writer.WriteChunk("chunk0", 0, nullptr, -1, nullptr), rdf::ApiException);
    }
}
