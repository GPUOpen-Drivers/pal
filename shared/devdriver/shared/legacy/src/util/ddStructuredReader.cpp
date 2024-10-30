/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <util/ddStructuredReader.h>
#include <util/vector.h>

#include <mpack.h>

// We'd use DD_WARN, but we want the parent function's location....
#define ResetInternalErrorState() ResetInternalErrorStateImpl(__FILE__, __LINE__, __FUNCTION__)

#if DD_PLATFORM_IS_UM

#define RAPIDJSON_PARSE_DEFAULT_FLAGS (kParseStopWhenDoneFlag | kParseCommentsFlag | kParseTrailingCommasFlag | kParseNanAndInfFlag)
#define RAPIDJSON_ASSERT(x) DD_ASSERT(x)
#if defined(__clang__)
    #pragma clang diagnostic ignored "-Wunknown-warning-option"
#endif
#include "rapidjson/reader.h"
#endif

namespace DevDriver
{
    const char* GetMpackErrorString(mpack_error_t error)
    {
        switch (error)
        {
            case mpack_ok:                  return "[mpack_ok] No error";
            case mpack_error_io:            return "[mpack_error_io] The reader or writer failed to fill or flush, or some other file or socket error occurred";
            case mpack_error_invalid:       return "[mpack_error_invalid] The data read is not valid MessagePack";
            case mpack_error_unsupported:   return "[mpack_error_unsupported] The data read is not supported by this configuration of MPack";
            case mpack_error_type:          return "[mpack_error_type] The type or value range did not match what was expected by the caller";
            case mpack_error_too_big:       return "[mpack_error_too_big] A read or write was bigger than the maximum size allowed for that operation";
            case mpack_error_memory:        return "[mpack_error_memory] An allocation failure occurred";
            case mpack_error_bug:           return "[mpack_error_bug] The MPack API was used incorrectly";
            case mpack_error_data:          return "[mpack_error_data] The contained data is not valid";
            case mpack_error_eof:           return "[mpack_error_eof] The reader failed to read because of file or socket EOF";
        }
        return "[???] Unrecognized mpack error";;
    }

    // RjReaderHandler processes rapidjson SAX tokens into messagepack.
    // rapidjson parses the Json and calls the appropriate method for each value (or key) that it finds, as it finds it.
    // This allows us to parse Json directly into messagepack.
    //
    // Implementation of rapidjson's Handler concept.
    //      See: http://rapidjson.org/classrapidjson_1_1_handler.html
    class RjReaderHandler
    {
    private:
        // We manually patch the messagepack as we parse the Json.
        // This struct keeps some metadata for each patch that allows us to double check our work.
        struct PatchInfo
        {
            enum Type
            {
                Other,
                // MsgPack stores 1 byte for the type and 4 bytes for a 32-bit length
                Array,
                // MsgPack stores 1 byte for the type and 2 bytes for a 16-bit length
                Object,
            };
            Type   type   = Type::Other;
            size_t offset = 0;
            size_t size   = 0;
        };
    public:
        // ==== Public Methods
        RjReaderHandler(const AllocCb& allocCb)
        : m_buffer(allocCb),
          m_openPatches(allocCb)
        {
            // Initialize the mpack writer to an invalid state.
            // This should catch missing calls RjReaderHandler::Init()
            mpack_writer_init_error(&m_writer, mpack_error_invalid);
        }

        // Initialize this Handler with a buffer large enough to parse the Json.
        // The buffer is allocated once and not resized, so this estimate must not under-estimate.
        // The input size of the Json is a reasonable estimate.
        Result Init(size_t sizeEstimate)
        {
            // Small size estimates can hit weird corner cases
            sizeEstimate = Platform::Max(static_cast<size_t>(16u), sizeEstimate);
            m_buffer.Resize(sizeEstimate);

            // TODO: mpack may have a mechanism to resize its internal buffer, but will take time to research and hook up.
            //       Until then, we over allocate a buffer upfront and shrink it later.
            mpack_writer_init(&m_writer, m_buffer.Data(), m_buffer.Size());
            return Result::Success;
        }

        // Destroys the internal messagepack writer object.
        // This must be called before calling `TakeBuffer()`.
        Result Finish()
        {
            // Shrink our buffer to the size that we actually used, but only if we still have a buffer.
            if (m_buffer.Size() != 0)
            {
                const size_t usedSize = mpack_writer_buffer_used(&m_writer);
                DD_ASSERT(usedSize <= m_buffer.Size());
                m_buffer.Resize(usedSize);
            }

            const mpack_error_t error = mpack_writer_destroy(&m_writer);
            DD_ASSERT(error == mpack_ok);
            return (error == mpack_ok) ? Result::Success : Result::Error;
        }

        // Reset internal state so that `Init()` can be called again.
        // After calling `Destroy()`, no methods can be called until `Init()` is called.
        void Destroy()
        {
            DD_UNHANDLED_RESULT(Finish());

            // If there are any patches still in this list, we probably didn't finish parsing.
            m_buffer.Clear();
            DD_WARN(m_openPatches.Size() == 0);
            m_openPatches.Clear();
            m_totalArrayPatches = 0;
            m_totalObjectPatches = 0;
        }

        // Give-up ownership of the vector, replacing it with an empty one
        // `Finish()` must be called before this is called.
        Vector<char>&& TakeBuffer()
        {
            return Platform::Move(m_buffer);
        }

        // Print interesting information about the json parsing.
        // This is most useful if called after `Finish()`.
        void PrintDebugStats() const
        {
            DD_PRINT(LogLevel::Debug, "Total Array  patches: %zu", m_totalArrayPatches);
            DD_PRINT(LogLevel::Debug, "Total Object patches: %zu", m_totalObjectPatches);
        }

        // ==== Concept Defines

        using Ch       = char;
        using SizeType = size_t;

        // ==== Concept Methods
        // These methods return true to continue the parsing process. Returning false immediately ends the parsing process.

        bool Null()
        {
            mpack_write_nil(&m_writer);
            return true;
        }

        bool Bool(bool value)
        {
            mpack_write_bool(&m_writer, value);
            return true;
        }

        bool Int(int value)
        {
            mpack_write_int(&m_writer, value);
            return true;
        }
        bool Uint(unsigned value)
        {
            mpack_write_uint(&m_writer, value);
            return true;
        }

        bool Int64(int64_t value)
        {
            mpack_write_i64(&m_writer, value);
            return true;
        }
        bool Uint64(uint64_t value)
        {
            mpack_write_u64(&m_writer, value);
            return true;
        }

        bool Double(double value)
        {
            mpack_write_double(&m_writer, value);
            return true;
        }

        // This definition is required but unused.
        // It's enabled via kParseNumbersAsStringsFlag and does NOT NULL-terminate `pUtf8`
        bool RawNumber(const Ch* pUtf8, SizeType length, bool copy)
        {
            DD_UNUSED(length);
            DD_UNUSED(copy);
            DD_PRINT(LogLevel::Always, "RawNumber: \"%.*s\"", length, pUtf8);
            DD_ASSERT_ALWAYS();
            return false;
        }

        bool String(const Ch* pStr, SizeType length, bool copy)
        {
            DD_UNUSED(copy);
            const bool valid = (length <= UINT32_MAX);

            if (valid)
            {
                const uint32 len = static_cast<uint32>(length);
                // Write the string and an extra NULL byte.
                mpack_start_str(&m_writer, len + 1);

                mpack_write_bytes(&m_writer, pStr, len);

                const char null = 0;
                mpack_write_bytes(&m_writer, &null, 1);

                mpack_finish_str(&m_writer);
            }

            return valid ;
        }

        bool StartObject()
        {
            bool valid = (mpack_writer_error(&m_writer) == mpack_ok);

            if (valid)
            {
                PatchInfo info;

                info.type = PatchInfo::Type::Object;
                info.offset = mpack_writer_buffer_used(&m_writer);
                // Store as a 16-bit number because it's smaller and unlikely to be too small.
                mpack_start_map(&m_writer, UINT16_MAX);
                info.size = mpack_writer_buffer_used(&m_writer) - info.offset;

                m_totalObjectPatches += 1;

                valid &= m_openPatches.PushBack(info);
                valid &= (mpack_writer_error(&m_writer) == mpack_ok);
            }

            return valid;
        }

        bool Key(const Ch* pStr, SizeType length, bool copy)
        {
            DD_UNUSED(copy);
            const bool valid = (length <= UINT32_MAX);

            if (valid)
            {
                mpack_write_str(&m_writer, pStr, static_cast<uint32>(length));
            }

            return valid;
        }

        bool EndObject(SizeType memberCount)
        {
            // We use a 16-bit int here instead of the maximum allowed because we want to save the space.
            // It's unlikely that a single object has over 65k members.
            bool valid = (memberCount < UINT16_MAX);

            if (valid)
            {
                const uint16 count = static_cast<uint16>(memberCount);
                if (mpack_writer_error(&m_writer) == mpack_ok)
                {
                    PatchInfo info;
                    m_openPatches.PopBack(&info);

                    LogPatchInfo(info);

                    DD_WARN(info.size == (sizeof(count) + 1));
                    valid &= (info.size == (sizeof(count) + 1));

                    DD_WARN(info.type == PatchInfo::Type::Object);
                    valid &= (info.type == PatchInfo::Type::Object);

                    DD_WARN(info.offset + info.size <= mpack_writer_buffer_used(&m_writer));
                    valid &= (info.offset + info.size <= mpack_writer_buffer_used(&m_writer));

                    if (valid)
                    {
                        // Store with the correct endianness 1 past the offset we stored
                        // The offset points to the tag byte of the map header.
                        mpack_store_u16(&m_buffer[info.offset + 1], count);
                        mpack_finish_map(&m_writer);
                    }
                }

                valid &= (mpack_writer_error(&m_writer) == mpack_ok);
            }
            else
            {
                DD_PRINT(LogLevel::Warn, "Translating Json to MessagePack failed when ending an object with %zu members", memberCount);
            }

            return valid;
        }

        bool StartArray()
        {
            bool valid = (mpack_writer_error(&m_writer) == mpack_ok);

            if (valid)
            {
                PatchInfo info;

                info.type = PatchInfo::Type::Array;
                info.offset = mpack_writer_buffer_used(&m_writer);
                // Store as a 32-bit number - mpack does not support larger
                mpack_start_array(&m_writer, UINT32_MAX);
                info.size = mpack_writer_buffer_used(&m_writer) - info.offset;

                m_totalArrayPatches += 1;

                valid &= m_openPatches.PushBack(info);
                valid &= (mpack_writer_error(&m_writer) == mpack_ok);
            }

            return valid;
        }

        bool EndArray(SizeType memberCount)
        {
            bool valid = (memberCount < UINT32_MAX);

            if (valid)
            {
                const uint32 count = static_cast<uint32>(memberCount);
                if (mpack_writer_error(&m_writer) == mpack_ok)
                {
                    PatchInfo info;
                    m_openPatches.PopBack(&info);

                    LogPatchInfo(info);

                    DD_WARN(info.size == (sizeof(count) + 1));
                    valid &= (info.size == (sizeof(count) + 1));

                    DD_WARN(info.type == PatchInfo::Type::Array);
                    valid &= (info.type == PatchInfo::Type::Array);

                    DD_WARN(info.offset + info.size <= mpack_writer_buffer_used(&m_writer));
                    valid &= (info.offset + info.size <= mpack_writer_buffer_used(&m_writer));

                    if (valid)
                    {
                        // Store with the correct endianness 1 past the offset we stored
                        // The offset points to the tag byte of the map header.
                        mpack_store_u32(&m_buffer[info.offset + 1], count);
                        mpack_finish_array(&m_writer);
                    }
                }

                valid &= (mpack_writer_error(&m_writer) == mpack_ok);
            }
            else
            {
                DD_PRINT(LogLevel::Warn, "Translating Json to MessagePack failed when ending an array with %zu members", memberCount);
            }

            return valid;
        }

    private:
        // Internal helper that prints an individual patch metadata, if enabled.
        static void LogPatchInfo(const PatchInfo& info)
        {
            constexpr LogLevel level = LogLevel::Never;
            if (DD_WILL_PRINT(level))
            {
                const char* pLabel = "";
                switch (info.type)
                {
                case PatchInfo::Type::Array:
                    pLabel = "Array";
                    break;
                case PatchInfo::Type::Object:
                    pLabel = "Object";
                    break;
                default:
                    // Do nothing - this shouldn't happen but will stand out in the list
                    break;
                }

                DD_PRINT(level, "PatchInfo { offset: %4zu, size: %4zu } %s",
                    info.offset,
                    info.size,
                    pLabel);
            }
            else
            {
                DD_UNUSED(info);
            }
        }

        /// mpack object that manages writing out messagepack into the buffer
        mpack_writer_t    m_writer;

        /// heap-allocated buffer in which to write messagepack
        /// Although this is a Vector, it is not resized after calling `Init()`.
        // TODO: Resize this dynamically as `m_writer` fills it up.
        //       mpack has support for this, but it will take some work to hook up and the benefits are not obvious.
        Vector<char>      m_buffer;

        /// A stack of patches that have not been "run".
        /// Each patch is pushed onto this stack when the object/array is opened, and popped when it closes.
        /// When parsing Json, there is only ever one array or object in the immediate scope, so this stack fits well.
        /// This should be empty when parsing is completed.
        Vector<PatchInfo> m_openPatches;

        /// Debug statistics
        size_t            m_totalArrayPatches = 0;
        size_t            m_totalObjectPatches   = 0;
    };

    // Concrete implementation of IStructuredReader, specializing in MessagePack
    class MessagePackReader final : public IStructuredReader
    {
    public:
        MessagePackReader() = delete;
        // Don't construct this on the stack
        MessagePackReader(const AllocCb& allocCb);

        MessagePackReader(MessagePackReader&&)            = delete;
        MessagePackReader& operator=(MessagePackReader&&) = delete;

        MessagePackReader(const MessagePackReader&)            = delete;
        MessagePackReader& operator=(const MessagePackReader&) = delete;

        virtual ~MessagePackReader() override {}

        // Initialize the reader and make it ready to parse the msgpack data
        Result Init(const uint8* pBytes, size_t numBytes);

        // Initialize the reader by taking ownership of a buffer
        // Use this when the allocation lifetime must be paired with the Reader.
        // This is common when cross-parsing - e.g. from Json into MessagePack.
        Result Init(Vector<char>&& buffer);

        // De-inits and deallocates the reader
        void Destroy()
        {
            const mpack_error_t error = mpack_tree_destroy(&m_tree);

            // We put in a lot of effort (too much?) to keep this error state clear, even when errors happen.
            // If this assert fires, we (the implementors of StructuredValue) have messed up badly and you will see odd bugs:
            //      1) Values you know are valid start returning NULL instead
            //      2) This behavior is consistent between runs, but noisy if you innocently reorder your code
            // Consult pfnMpackErrorCb in Init() to help track down the issue.
            DD_ASSERT(error == mpack_ok);

            if (error != mpack_ok)
            {
                DD_PRINT(LogLevel::Debug,
                         "[IStructuredReader] mpack_tree_destroy() returned error %d: %s",
                         static_cast<uint32>(error),
                         GetMpackErrorString(error));
            }
        }

        // Crate a value at the root of the messagepack document
        StructuredValue GetRoot() const override;

        // Access allocation callbacks
        const AllocCb& GetAllocCb() const override;

    private:
        // Allocation Callbacks
        const AllocCb&       m_allocCb;

        // If this MesssagePackReader was created from some other format, it owns the messagepack data.
        Vector<char>         m_scratch;

        // Metadata for parsing a message pack buffer
        // mpack library is not const-correct, so we mark our api handle mutable
        mutable mpack_tree_t m_tree;
    };

    // Opaque to mpack_node_t conversion
    static_assert(sizeof(StructuredValue::OpaqueNode)  == sizeof(mpack_node_t),  "StructuredValue::OpaqueNode size doesn't match mpack_node_t. Please update the header.");
    static_assert(alignof(StructuredValue::OpaqueNode) == alignof(mpack_node_t), "StructuredValue::OpaqueNode align doesn't match mpack_node_t. Please update the header.");

    // Unpack a node from out opaque format
    static mpack_node_t UnpackNode(const StructuredValue::OpaqueNode& opaque)
    {
        // Sanity checks
        DD_ASSERT(opaque.blob[0] != nullptr);
        DD_ASSERT(opaque.blob[1] != nullptr);

        mpack_node_t node;
        memcpy(&node, &opaque, sizeof(node));

        if (node.tree->error != mpack_ok)
        {
            DD_PRINT(LogLevel::Debug, "[%s] %s", __FUNCTION__, GetMpackErrorString(node.tree->error));
            // We shouldn't hit this code, but uncommenting the following line breaks tests.
            // node.tree->error = mpack_ok;
        }

        return node;
    }

    // Pack a node into our Opaque format
    static StructuredValue::OpaqueNode PackNode(mpack_node_t node)
    {
        if (node.tree->error != mpack_ok)
        {
            mpack_error_t error = node.tree->error;
            DD_PRINT(LogLevel::Debug, "node.tree->error = %d (0x%x) %s", error, error, GetMpackErrorString(error));
            node.tree->error = mpack_ok;
        }

        StructuredValue::OpaqueNode opaque;
        memcpy(reinterpret_cast<void*>(&opaque), &node, sizeof(node));

        // Sanity checks
        DD_ASSERT(opaque.blob[0] != nullptr);
        DD_ASSERT(opaque.blob[1] != nullptr);

        return opaque;
    }

    MessagePackReader::MessagePackReader(const AllocCb& allocCb)
        : m_allocCb(allocCb),
          m_scratch(allocCb)
    {
        // Initialize the mpack reader to an invalid state.
        // This should catch missing calls MessagePackReader::Init()
        mpack_tree_init_error(&m_tree, mpack_error_invalid);
    }

    Result MessagePackReader::Init(const uint8* pBytes, size_t numBytes)
    {
        Result result = Result::InvalidParameter;

        if ((pBytes != nullptr) && (numBytes != 0))
        {
            result = Result::Success;
        }

        if (result == Result::Success)
        {
            // Initialize the mpack tree from out existing buffer.
            // TODO: This can allocate, we need to pre-allocate nodes on mpack's behalf
            mpack_tree_init_data(&m_tree, reinterpret_cast<const char*>(pBytes), numBytes);

            // Set an error callback
            // This is called whenever mpack hits an error state. We overwrite that state, so this fires excessively,
            // but can still be helpful for debugging.
            mpack_tree_error_t pfnMpackErrorCb= [](mpack_tree_t* tree, mpack_error_t error) {
                // We pass the whole reader object to this function, but do not use it yet.
                MessagePackReader* pReader = reinterpret_cast<MessagePackReader*>(tree->context);
                DD_UNUSED(pReader);

                // If you're here debugging something, break point here.

                DD_PRINT(LogLevel::Debug, "%s", GetMpackErrorString(error));
            };
            mpack_tree_set_context(&m_tree, this);
            mpack_tree_set_error_handler(&m_tree, pfnMpackErrorCb);

            mpack_tree_parse(&m_tree);
            const mpack_error_t error = mpack_tree_error(&m_tree);

            result = (error == mpack_ok) ? Result::Success : Result::InvalidParameter;
        }

        return result;
    }

    Result MessagePackReader::Init(Vector<char>&& buffer)
    {
        Result result = Result::InvalidParameter;

        if (buffer.Size() != 0)
        {
            m_scratch = Platform::Move(buffer);
            result = Result::Success;
        }

        if (result == Result::Success)
        {
            result = Init(reinterpret_cast<const uint8*>(m_scratch.Data()), m_scratch.Size());
        }

        return result;
    }

    StructuredValue MessagePackReader::GetRoot() const
    {
        return StructuredValue(PackNode(mpack_tree_root(&m_tree)));
    }

    const AllocCb& MessagePackReader::GetAllocCb() const
    {
        return m_allocCb;
    }

#if DD_PLATFORM_IS_UM
    static const char* ParseErrorCodeToString(rapidjson::ParseErrorCode code)
    {
        switch (code)
        {
            case rapidjson::kParseErrorDocumentEmpty:                  return "The document is empty";
            case rapidjson::kParseErrorDocumentRootNotSingular:        return "The document root must not follow by other values";
            case rapidjson::kParseErrorValueInvalid:                   return "Invalid value";
            case rapidjson::kParseErrorObjectMissName:                 return "Missing a name for object member";
            case rapidjson::kParseErrorObjectMissColon:                return "Missing a colon after a name of object member";
            case rapidjson::kParseErrorObjectMissCommaOrCurlyBracket:  return "Missing a comma or '}' after an object member";
            case rapidjson::kParseErrorArrayMissCommaOrSquareBracket:  return "Missing a comma or ']' after an array element";
            case rapidjson::kParseErrorStringUnicodeEscapeInvalidHex:  return "Incorrect hex digit after \\u escape in string";
            case rapidjson::kParseErrorStringUnicodeSurrogateInvalid:  return "The surrogate pair in string is invalid";
            case rapidjson::kParseErrorStringEscapeInvalid:            return "Invalid escape character in string";
            case rapidjson::kParseErrorStringMissQuotationMark:        return "Missing a closing quotation mark in string";
            case rapidjson::kParseErrorStringInvalidEncoding:          return "Invalid encoding in string";
            case rapidjson::kParseErrorNumberTooBig:                   return "Number too big to be stored in double";
            case rapidjson::kParseErrorNumberMissFraction:             return "Miss fraction part in number";
            case rapidjson::kParseErrorNumberMissExponent:             return "Miss exponent in number";
            case rapidjson::kParseErrorTermination:                    return "Parsing was terminated";
            case rapidjson::kParseErrorUnspecificSyntaxError:          return "Unspecific syntax error";
            case rapidjson::kParseErrorNone:
            {
                DD_WARN_REASON("ParseErrorCodeToString was called with kParseErrorNone");
                return "No error";
            }
        }
        return "Unrecognized parse error";
    }

    static void PrintDetailedJsonParseError(
        const rapidjson::ParseResult& parseResult,
        const char*                   pJsonText,
        size_t                        textSize,
        const AllocCb&                allocCb)
    {
        // See the rapidjson docs for details on these.
        // http://rapidjson.org/group___r_a_p_i_d_j_s_o_n___e_r_r_o_r_s.html#structrapidjson_1_1_parse_result
        const auto errorCode = parseResult.Code();
        const char* pParseErrorCodeString = ParseErrorCodeToString(errorCode);

        const size_t errorLoc = parseResult.Offset();

        // Grab some context around the problem point

        // Line number of error - 1-indexed like your text editor
        size_t errorLineNum = 1;
        size_t errorColmNum = 1;
        // Save the index of the last two lines that we find.
        // This will give us the index of the begining of the line with the error, but also
        // the line immediately after.
        // This lets us print more context about the json.
        size_t whichLine = 0;
        size_t lineStarts[2] = { 0, 0 };
        for (size_t i = 0; i < errorLoc; i += 1)
        {
            errorColmNum += 1;
            if (pJsonText[i] == '\n')
            {
                errorLineNum += 1;
                errorColmNum = 1;
                // Save the offset for that line.
                lineStarts[whichLine] = i + 1;
                // And advance to the "next" save location.
                whichLine = (whichLine + 1) % Platform::ArraySize(lineStarts);
            }
        }

        // On Debug builds, we print detailed error messages with context lines and a little "^".
        // This is a lot of string manipulation that we do not want on for Release builds, where it's a bit riskier.
#if defined(_DEBUG)
        DD_UNUSED(errorColmNum);

        {
            // Sort our list so that they're in the correct order.
            {
                size_t min = Platform::Min(lineStarts[0], lineStarts[1]);
                size_t max = Platform::Max(lineStarts[0], lineStarts[1]);
                lineStarts[0] = min;
                lineStarts[1] = max;
            }

            size_t estimateToPrint = 0;
            for (size_t i = lineStarts[0], lineCounts = 0;
                (i < textSize) && (lineCounts < 3);
                i += 1)
            {
                if (pJsonText[i] == '\n')
                {
                    lineCounts += 1;
                    // We print another 6 ("%6zu") + 2 (": ") + 1 "\n" == 9 per line
                    estimateToPrint += 9;
                }
                estimateToPrint += 1;
            }
            // The longest error we receive from rapidjson is under 100 characters, so append that length to our estimate
            estimateToPrint += 100;

            // Calculate line lengths
            // These correspond with lineStarts above
            size_t lineLengths[2] = { 0, 0 };
            for (size_t line = 0; line < Platform::ArraySize(lineStarts); line += 1)
            {
                for (size_t i = lineStarts[line]; (i < textSize) && (pJsonText[i] != '\n'); i += 1)
                {
                    lineLengths[line] += 1;
                }
            }

            if (estimateToPrint < 1024)
            {
                Vector<char> buffer(allocCb);
                // Allocate our estimate
                // Resize returns void, so I sure hope this succeeds.
                buffer.Resize(estimateToPrint);

                size_t offset = 0;

                // Context
                offset += Platform::Snprintf(&buffer[offset], buffer.Size() - offset,
                    "%6zu: %.*s\n",
                    errorLineNum - 1,
                    lineLengths[0],
                    &pJsonText[lineStarts[0]]
                );
                offset -= 1; // Redact the NULL

                 // Error line
                offset += Platform::Snprintf(&buffer[offset], buffer.Size() - offset,
                    "%6zu: %.*s\n",
                    errorLineNum,
                    lineLengths[1],
                    &pJsonText[lineStarts[1]]
                );
                offset -= 1; // Redact the NULL

                // Line pointing to error
                const size_t carrotIndent = (errorLoc - lineStarts[1]) + 6 /*"%6zu"*/ + 2 /*": "*/;
                offset += Platform::Snprintf(&buffer[offset], buffer.Size() - offset,
                    "%*s^ %s\n",
                    carrotIndent,
                    " ",
                    pParseErrorCodeString
                );
                offset -= 1; // Redact the NULL

                DD_PRINT(LogLevel::Error, "Error parsing Json:\n%s", buffer.Data());
            }
            else
            {
                const size_t stringOffset = Platform::Min(errorLoc - 10, errorLoc);
                DD_PRINT(LogLevel::Error, "Json Parsing Error \"%s\" in \"%.10s\"",
                         pParseErrorCodeString,
                         &pJsonText[stringOffset]);
            }
        }
#else
        {
            DD_UNUSED(textSize);
            DD_UNUSED(allocCb);

            DD_PRINT(LogLevel::Error, "[IStructuredReader::CreateFromJson] Json Parse Error at in.json:%zu:%zu: %s",
                errorLineNum,
                errorColmNum,
                pParseErrorCodeString);
            DD_PRINT(LogLevel::Error, "[IStructuredReader::CreateFromJson] Rerun in a debug build for more detailed error information");
        }
#endif
    }

    Result IStructuredReader::CreateFromJson(
        const void*         pBytes,
        size_t              numBytes,
        const AllocCb&      allocCb,
        IStructuredReader** ppReader
    )
    {
        Result result = Result::InvalidParameter;

        if ((pBytes != nullptr) && (numBytes > 0) && (ppReader != nullptr))
        {
            result = Result::Success;
        }

        // Cross-parse the Json into MessagePack, and then use the MessagePack reader.
        // This is fine - if we wrote a dedicated JsonReader we may be tempted to store the Json in a compressed format
        // in-memory... which is what MessagePack is.
        // So we skip the middle layer and cross-parse directly into messagepack.
        // MessagePackReader is appropriately modified to conditionally own a buffer.
        Vector<char> msgpackBuffer(allocCb);
        if (result == Result::Success)
        {
            // Our `handler` object will receive SAX events from rapidjson and write out messagepack.
            RjReaderHandler handler(allocCb);
            if (result == Result::Success)
            {
                // TODO: Revisit this size estimate
                // This design was originally written expecting MessagePack to *always* be smaller than Json.
                // In practice, this doesn't happen because of our patching of the messagepack data that RjHandler does.
                // This means we need to estimate *more* space than the text size takes up. We use 2x as an excessive
                // estimate, just to be safe. We later shrink the Vector, but it's not clear if that actually
                // deallocates anything.
                //
                // We need to review this and generate a better estimate - ideally we'd do two passes over the Json.
                //      1. The first pass is used to estimate the size needed,
                //          and avoid the use of the patching scheme we have.
                //      2. The second pass then writes out all of the data, after allocating it exactly.
                //
                // For now just double the size and know that it'll be "good enough".
                const size_t messagepackSizeEstimate = 2 * numBytes;
                result = handler.Init(messagepackSizeEstimate);
            }

            if (result == Result::Success)
            {
                rapidjson::MemoryStream memoryStream(reinterpret_cast<const char*>(pBytes), numBytes);
                rapidjson::EncodedInputStream<rapidjson::UTF8<char>, rapidjson::MemoryStream> stream(memoryStream);

                rapidjson::Reader reader(nullptr, 0);
                rapidjson::ParseResult parseResult = reader.Parse(stream, handler);

                if (parseResult.IsError())
                {
                    // Invalid Json is an invalid parameter
                    result = Result::InvalidParameter;

                    // This is potentially quite expensive, so guard it behind an error level check
                    if (DD_WILL_PRINT(LogLevel::Error))
                    {
                        PrintDetailedJsonParseError(parseResult, reinterpret_cast<const char*>(pBytes), numBytes, allocCb);
                    }
                }
                else
                {
                    result = handler.Finish();
                }
            }

            if (result == Result::Success)
            {
                // Move the buffer out of the handler. Our `MessagePackReader` is going to need to own this
                // allocation so that it lives long enough.
                msgpackBuffer = handler.TakeBuffer();
                handler.PrintDebugStats();
            }

            handler.Destroy();
        }

        MessagePackReader* pReader = nullptr;
        if (result == Result::Success)
        {
            DD_PRINT(LogLevel::Verbose,
                     "[IStructuredReader::CreateFromJson] Parsed %zu bytes of Json into %zu bytes of MessagePack",
                     numBytes,
                     msgpackBuffer.Size());
            pReader = DD_NEW(MessagePackReader, allocCb)(allocCb);
            result = (pReader != nullptr) ? Result::Success : Result::InsufficientMemory;
        }

        if (result == Result::Success)
        {
            if (pReader != nullptr)
            {
                // `pReader` must own this allocation
                result = pReader->Init(Platform::Move(msgpackBuffer));
            }
            else
            {
                DD_ASSERT_ALWAYS();
                result = Result::MemoryOverLimit;
            }
        }

        if (result == Result::Success)
        {
            // This ASSERT should have been checked already, but we leave it here in case the code changes.
            DD_ASSERT(pReader != nullptr);
            *ppReader = pReader;
        }
        else
        {
            // If anything went wrong anywhere, make sure that we clean this up!
            DD_DELETE(pReader, allocCb);
        }

        return result;
    }
#endif

    Result IStructuredReader::CreateFromMessagePack(
        const uint8*        pBuffer,
        size_t              bufferSize,
        const AllocCb&      allocCb,
        IStructuredReader** ppReader
    )
    {
        Result result = Result::InvalidParameter;

        if ((pBuffer != nullptr) && (bufferSize != 0))
        {
            result = Result::Success;
        }

        MessagePackReader* pReader = nullptr;
        if (result == Result::Success)
        {
            pReader = DD_NEW(MessagePackReader, allocCb)(allocCb);
            result = (pReader != nullptr) ? Result::Success : Result::InsufficientMemory;
        }

        if (result == Result::Success)
        {
            if (pReader != nullptr)
            {
                result = pReader->Init(pBuffer, bufferSize);
            }
            else
            {
                DD_ASSERT_ALWAYS();
                result = Result::MemoryOverLimit;
            }
        }

        if (result == Result::Success)
        {
            DD_ASSERT(pReader != nullptr);
            *ppReader = pReader;
        }
        else
        {
            DD_DELETE(pReader, allocCb);
            // It's likely that the caller didn't bother initializing their pointer.
            // Just in case, we do it here to prevent hard-to-debug crashes.
            if (ppReader != nullptr)
            {
                *ppReader = nullptr;
            }
        }

        return result;
    }

    void IStructuredReader::Destroy(IStructuredReader** ppReader)
    {
        DD_WARN(ppReader != nullptr);
        if (ppReader != nullptr)
        {
            DD_WARN(*ppReader != nullptr);
            auto* pReader = static_cast<MessagePackReader*>(*ppReader);
            if (pReader != nullptr)
            {
                pReader->Destroy();
                DD_DELETE(pReader, pReader->GetAllocCb());
            }
        }
    }

    // Resets the mpack global error state if set, and returns true when the StructuredValue's node is in a "good" state.
    // "good" state means: no errors before the reset and the node contains some kind of value.
    bool StructuredValue::ResetInternalErrorStateImpl(const char* pFile, int line, const char* pCallingFunction) const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const mpack_error_t error = mpack_node_error(node);
        const bool ok = (error == mpack_ok);

        if (!ok)
        {
            // This may help debug bad parses
            DD_PRINT(LogLevel::Debug, "%s:%d %s: mpack node error \"%s\"",
                pFile,
                line,
                pCallingFunction,
                GetMpackErrorString(error));
        }
        // Reset the global error state so that future calls work.
        // This is not something we "should" be doing, but to get the node api to work how we want we must.
        //      Our StructuredValue api needs to work even after encountering an error.
        //      mpack's node api is designed for a lot of reads and error checking at the end.
        node.tree->error = mpack_ok;

        return ok;
    }

    StructuredValue::Type StructuredValue::GetType() const
    {
        const mpack_node_t node = UnpackNode(m_opaque);

        switch (mpack_node_type(node))
        {
            case mpack_type_bool:    return Type::Bool;

            case mpack_type_int:     return Type::Int;
            case mpack_type_uint:    return Type::Uint;

            case mpack_type_float:   return Type::Float;
            case mpack_type_double:  return Type::Double;

            case mpack_type_str:     return Type::Str;
            case mpack_type_array:   return Type::Array;
            case mpack_type_map:     return Type::Map;

            case mpack_type_bin:
                DD_ASSERT_REASON("Unexpected 'bin' value in mpack data");
                DD_FALLTHROUGH();
            case mpack_type_missing: DD_FALLTHROUGH();
            case mpack_type_nil:     return Type::Null;

            default:                 return Type::Null;
        }
    }

    // Public StructuredValue methods that wrap mpack functions
    //  These try to be faithful mappings that do very little extra checking.
    //  The one thing that they must all do is call ResetInternalErrorState().
    //  mpack is design

    // This cannot be static because it requires an existing node.
    // Null nodes in mpack reference the same global tree.
    StructuredValue StructuredValue::MakeNull() const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        // This is an internal mpack function, but we need a way to create Null nodes
        return StructuredValue(PackNode(mpack_tree_nil_node(node.tree)));
    }

    bool StructuredValue::IsNull() const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        return (mpack_node_is_nil(node) || mpack_node_is_missing(node));
    }

    bool StructuredValue::GetBool(bool* pValue) const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const bool value = mpack_node_bool(node);

        if ((pValue != nullptr) && (mpack_node_error(node) == mpack_ok))
        {
            *pValue = value;
        }

        return ResetInternalErrorState();
    }

    bool StructuredValue::GetUint64(uint64* pNum) const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const uint64 num = mpack_node_u64(node);

        if ((pNum != nullptr) && (mpack_node_error(node) == mpack_ok))
        {
            *pNum = num;
        }

        return ResetInternalErrorState();
    }

    bool StructuredValue::GetUint32(uint32* pNum) const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const uint32 num = mpack_node_u32(node);

        if ((pNum != nullptr) && (mpack_node_error(node) == mpack_ok))
        {
            *pNum = num;
        }

        return ResetInternalErrorState();
    }

    bool StructuredValue::GetUint16(uint16* pNum) const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const uint16 num = mpack_node_u16(node);

        if ((pNum != nullptr) && (mpack_node_error(node) == mpack_ok))
        {
            *pNum = num;
        }

        return ResetInternalErrorState();
    }

    bool StructuredValue::GetUint8(uint8* pNum) const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const uint8 num = mpack_node_u8(node);

        if ((pNum != nullptr) && (mpack_node_error(node) == mpack_ok))
        {
            *pNum = num;
        }

        return ResetInternalErrorState();
    }

    bool StructuredValue::GetInt64(int64* pNum) const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const int64 num = mpack_node_i64(node);

        if ((pNum != nullptr) && (mpack_node_error(node) == mpack_ok))
        {
            *pNum = num;
        }

        return ResetInternalErrorState();
    }

    bool StructuredValue::GetInt32(int32* pNum) const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const int32 num = mpack_node_i32(node);

        if ((pNum != nullptr) && (mpack_node_error(node) == mpack_ok))
        {
            *pNum = num;
        }

        return ResetInternalErrorState();
    }

    bool StructuredValue::GetInt16(int16* pNum) const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const int16 num = mpack_node_i16(node);

        if ((pNum != nullptr) && (mpack_node_error(node) == mpack_ok))
        {
            *pNum = num;
        }

        return ResetInternalErrorState();
    }

    bool StructuredValue::GetInt8(int8* pNum) const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const int8 num = mpack_node_i8(node);

        if ((pNum != nullptr) && (mpack_node_error(node) == mpack_ok))
        {
            *pNum = num;
        }

        return ResetInternalErrorState();
    }

    bool StructuredValue::GetFloat(float* pNum) const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const float num = mpack_node_float_strict(node);

        if ((pNum != nullptr) && (mpack_node_error(node) == mpack_ok))
        {
            *pNum = num;
        }

        return ResetInternalErrorState();
    }

    bool StructuredValue::GetDouble(double* pNum) const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const double num = mpack_node_double_strict(node);

        if ((pNum != nullptr) && (mpack_node_error(node) == mpack_ok))
        {
            *pNum = num;
        }

        return ResetInternalErrorState();
    }

    DD_NODISCARD bool StructuredValue::GetStringCopy(char* pBuffer, size_t bufferSize, size_t* pStringSize) const
    {
        const mpack_node_t node  = UnpackNode(m_opaque);
        const char*        pUtf8 = mpack_node_str(node);
        const size_t       len   = mpack_node_strlen(node);

        if (mpack_node_error(node) == mpack_ok)
        {
            // Is this already NULL terminated in the messagepack buffer?
            bool needsNull = true;

            if ((pUtf8 != nullptr) && (pUtf8[len - 1] == '\0'))
            {
                needsNull = false;
            }

            if ((pStringSize != nullptr))
            {
                if (needsNull)
                {
                    *pStringSize = len;
                }
                else
                {
                    // We don't need a NULL because we already have one, so `len` here includes that.
                    // Decrement that length because mpack tried to account for that and we're not double counting.
                    *pStringSize = len - 1;
                }
            }

            // mpack needs at least enough space to write a single NULL terminator byte
            if (bufferSize > 0)
            {
                if (needsNull)
                {
                    // Our string does NOT have a NULL so let mpack add one when writing.
                    // (and handle bad buffer sizes for us)
                    mpack_node_copy_cstr(node, pBuffer, bufferSize);
                }
                else
                {
                    // Our "utf8" string is alreay terminated, so we don't use the *_cstr() functions.
                    // This prevents writing double NULL terminators or other weird edge errors.
                    mpack_node_copy_utf8(node, pBuffer, bufferSize);
                }
            }
        }

        return ResetInternalErrorState();
    }

    DD_NODISCARD const char* StructuredValue::GetStringPtr() const
    {
        const mpack_node_t node  = UnpackNode(m_opaque);
        const size_t       len   = mpack_node_strlen(node);
        const char*        pUtf8 = mpack_node_str(node);

        bool valid = false;
        if (mpack_node_error(node) == mpack_ok)
        {
            // We can return a raw pointer iff the final byte in the string is NULL.
            // It may be the case that there are other NULLs earlier in the string: too bad. That's a programmer error.
            if ((pUtf8 != nullptr) && (pUtf8[len - 1] == '\0'))
            {
                valid = true;
            }
        }

        // This one function departs from the usual `return ResetInternalErrorState();` pattern because it has a
        // precondition that mpack doesn't know about - the NULL terminator existing in the mpack data.
        // We already checked it, but need to include it in the return value.
        valid &= ResetInternalErrorState();

        return (valid ? pUtf8 : nullptr);
    }

    bool StructuredValue::GetValueByKey(const char* pKey, StructuredValue* pValue) const
    {
        bool success = false;

        if (pValue != nullptr)
        {
            const mpack_node_t node     = UnpackNode(m_opaque);
            const mpack_node_t new_node = mpack_node_map_cstr_optional(node, pKey);

            (*pValue) = StructuredValue(PackNode(new_node));

            success = (ResetInternalErrorState() && (pValue->GetType() != Type::Null));
        }

        return success;
    }

    bool StructuredValue::GetValueByIndex(size_t index, StructuredValue* pValue) const
    {
        bool success  = false;

        if (pValue != nullptr)
        {
            const mpack_node_t node     = UnpackNode(m_opaque);
            const mpack_node_t new_node = mpack_node_array_at(node, index);

            (*pValue) = StructuredValue(PackNode(new_node));

            success = (ResetInternalErrorState() && (pValue->GetType() != Type::Null));
        }

        return success;
    }

    bool StructuredValue::IsMap() const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        return (mpack_node_type(node) == mpack_type_map);
    }

    bool StructuredValue::IsArray() const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        return (mpack_node_type(node) == mpack_type_array);
    }

    size_t StructuredValue::GetArrayLength() const
    {
        const mpack_node_t node = UnpackNode(m_opaque);
        const size_t length = mpack_node_array_length(node);
        ResetInternalErrorState();
        return length;
    }

}
