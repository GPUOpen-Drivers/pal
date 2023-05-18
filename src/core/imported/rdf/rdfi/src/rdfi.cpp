/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include <cli11/CLI11.hpp>
#include <json/json.hpp>

#include "amdrdf.h"

namespace
{
int PrintChunkInfo(const std::string& input, bool outputJson)
{
    rdf::ChunkFile chunkFile(input.c_str());

    auto it = chunkFile.GetIterator();

    nlohmann::json jsonSummary;
    jsonSummary["chunks"] = nlohmann::json();

    for (;;) {
        if (it.IsAtEnd()) {
            break;
        }

        char identifier[RDF_IDENTIFIER_SIZE + 1] = {};
        it.GetChunkIdentifier(identifier);
        const auto index = it.GetChunkIndex();

        const auto dataSize = chunkFile.GetChunkDataSize(identifier, index);
        const auto headerSize = chunkFile.GetChunkHeaderSize(identifier, index);
        const auto version = chunkFile.GetChunkVersion(identifier, index);

        if (outputJson) {
            jsonSummary["chunks"].push_back(
                {{"id", identifier},
                 {"index", index},
                 {"info",
                  {{"dataSize", dataSize}, {"headerSize", headerSize}, {"version", version}}}});
        } else {
            if (chunkFile.GetChunkCount(identifier) > 1) {
                std::cout << "ID: " << identifier << "[" << index << "]\n";
            } else {
                std::cout << "ID: " << identifier << "\n";
            }
            std::cout << "  Data size:   " << dataSize << "\n";
            std::cout << "  Header size: " << headerSize << "\n";
            std::cout << "  Version    : " << version << "\n";
        }

        it.Advance();
    }

    if (outputJson) {
        std::cout << jsonSummary.dump(4);
    }

    return 0;
}
}  // namespace

int main(int argc, char* argv[])
{
    CLI::App app{"RDFI 1.0"};

    std::string input;
    bool outputJson = false;

    auto printChunkInfo =
        app.add_subcommand("print-chunk-info", "Print information about all chunks in a file.");
    printChunkInfo->add_option("input", input)->required();
    printChunkInfo->add_flag("-j,--json", outputJson);

    CLI11_PARSE(app, argc, argv);

    try {
        if (*printChunkInfo) {
            return PrintChunkInfo(input, outputJson);
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
    }

    return 0;
}
