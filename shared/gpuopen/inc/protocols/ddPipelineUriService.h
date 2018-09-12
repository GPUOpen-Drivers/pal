/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "util/ddByteReader.h"

namespace DevDriver
{

static constexpr const char*        kPipelineUriServiceName    = "pipeline";
static constexpr DevDriver::Version kPipelineUriServiceVersion = 2;

// This struct exists to service a few issues:
//      1) We need a 128bit data type to represent Pipeline Hashes
//      2) Pal and Gpuopen have different Metrohash wrappers
//      3) Gpuopen's, with CMake, hides the metrohash.h as a "private" include,
//              so we can't even include it in the tests/tools.
// This struct can be constructed from the obvious numeric types, or implicitly from any 16-byte data type.
struct PipelineHash
{
    union
    {
        uint8  bytes [16];
        uint16 words [8];
        uint32 dwords[4];
        uint64 qwords[2];
        // There is no dqword member because:
        //      1) msvc doesn't expose an `unsigned __int128` (note: gcc and clang do)
        //      2) It would force our alignment requirement up to 16 bytes from 8 bytes.
        //         This would make PipelineRecordHeader 32 bytes and padded, instead of 24 and packed.
    };

    constexpr          PipelineHash()                          : qwords{ 0 } {}
    constexpr          PipelineHash(const PipelineHash& other) : qwords{ other.qwords[0], other.qwords[1] } {}
    constexpr explicit PipelineHash(uint8  num)                : bytes { num } {}
    constexpr explicit PipelineHash(uint16 num)                : words { num } {}
    constexpr explicit PipelineHash(uint32 num)                : dwords{ num } {}
    constexpr explicit PipelineHash(uint64 num)                : qwords{ num } {}
    constexpr explicit PipelineHash(int32  num)                : dwords{ static_cast<uint32>(num) } {}

    // All objects that are exactly 16-bytes can be implicitly converted into a PipelineHash.
    // This hides the fact that different components use different PipelineHash structures. Other components can just
    // use their type wherever a DevDriver::PipelineHash is needed, and it will implicitly convert.
    template<typename T, typename = typename Platform::EnableIf<sizeof(T) == 16>::Type>
    PipelineHash(const T& other)
    {
        memcpy(this, &other, sizeof(T));
    }
};
DD_CHECK_SIZE(PipelineHash, 16);

constexpr bool operator==(const PipelineHash& lhs, const PipelineHash& rhs)
{
    return lhs.qwords[0] == rhs.qwords[0] &&
           lhs.qwords[1] == rhs.qwords[1];
}

constexpr bool operator!=(const PipelineHash& lhs, const PipelineHash& rhs)
{
    return lhs.qwords[0] != rhs.qwords[0] ||
           lhs.qwords[1] != rhs.qwords[1];
}

// Some metadata for a pipeline code object.
struct PipelineRecordHeader
{
    PipelineHash hash;     //< Pipeline hash
    uint64       size = 0; //< Size in bytes of the pipeline binary data
};
DD_CHECK_SIZE(PipelineRecordHeader, 24);

// A reference to a pipeline code object and some of its metadata.
struct PipelineRecord
{
    PipelineRecordHeader header;            //< Pipeline metadata
    const void*          pBinary = nullptr; //< Pipeline binary data
};

// An object used to iterate over a serialized list of PipelineRecords.
// The iterator will deserialize PipelineRecords until there are no more
// available, or until it encounteres an error reading a record. At either point,
// we say that the iterator is "exhausted", and it will never produce more items.
class PipelineRecordsIterator
{
public:
    // Construct an iterator over a list of PipelineRecords, stored according to
    // the POST data format of the pipeline://reinject URI request.
    PipelineRecordsIterator(const void* pBlobBegin, size_t blobSize);

    // If the iterator has not been exhausted, write a PipelineRecord through `pRecord` and returns true.
    // If the iterator has been exhausted, return false and do not write anything through `pRecord`.
    // If `pRecord` is NULL, no PipelineRecord is written, but Get() operates the same as it otherwise would have.
    bool Get(PipelineRecord* pRecord) const;

    // Provide access to the last PipelineRecord generated by this iterator.
    const PipelineRecord* operator->() const {
        DD_ASSERT(m_lastResult == Result::Success);
        return &m_record;
    }

    // Advance this iterator to the next available PipelineRecord.
    // When the iterator is exhausted, this method has no effect.
    void Next();

    // Return the last error encountered. This is intended to be used internally by the pipeline service.
    // Drivers should prefer `Get()` to determine if there is a PipelineRecord available.
    // This is:
    //      Result::Success if there is a current PipelineRecord available through `Get()`,
    //      Result::EndOfStream when the iterator has been exhausted without issue,
    //      or any other Result value when an error has been encountered with the Uri POST data.
    Result GetLastError() const { return m_lastResult; }

private:
    PipelineRecord  m_record;
    ByteReader      m_reader;
    Result          m_lastResult;
};

// Flags used to exclude certain pipelines
struct ExclusionFlags
{
    union {
        struct {
            DevDriver::uint64 reserved : 64;
        };
        DevDriver::uint64 allFlags;
    };

    ExclusionFlags()
        : allFlags(0)
    {}
};
static_assert(sizeof(ExclusionFlags) == sizeof(ExclusionFlags::allFlags),
    "DevDriver::ExclusionFlags structure is broken - does Flags::allFlags need to be updated?");

class PipelineUriService : public IService
{
public:
    // ===== Callback Function Definitions ============================================================================

    // Drivers call this function to build the hash index to be sent to the consumer.
    // This function should only be called during the GetPipelineHashes() callback.
    void AddHash(const PipelineHash& hash, uint64 size);

    // Drivers call this function to add code objects to the list being sent to the consumer.
    // This function should only be called during the GetPipelineCodeObjects() callback.
    void AddPipeline(const PipelineRecord& pipeline);

    // Overview:
    //      Clients provide an implementation of this function if they wish to support queries for
    //      an index of available pipelines.
    // Request Format:
    //      uri request:
    //          pipeline://getIndex [exclusionFlags]
    //      uri arguments:
    //          exclusionFlags  - A hex-encoded bitfield indicating which types of pipelines to exclude.
    //                            See the ExclusionFlags struct for details.
    //      POST data:
    //          None
    // Response Data Format:
    //      A serialized array of PipelineRecordHeaders.
    //      <For each Pipeline Record>
    //          [uint64]        A 128 Pipeline Hash
    //          [uint64]            cont.
    //          [uint64]        The number of bytes in the pipeline code object data, stored in little endian.
    //      </Pipeline Record>
    // Callback Paramaters:
    //      pService            - The service passes itself for access to the Add*() callbacks.
    //      pUserData           - The Driver's user data pointer, initialized with the Service.
    //      flags               - Exclusion flags to limit which pipelines are returned.
    // Return Values:
    //      Result::Success     - if an index of available pipelines was able to be generated
    //      Otherwise, Driver defined Result values.
    typedef DevDriver::Result (GetPipelineHashes)(PipelineUriService* pService,
                                                  void*               pUserData,
                                                  ExclusionFlags      flags);

    // Overview:
    //      Clients provide an implementation of this function if they wish to support dumping pipeline code objects.
    //      Pipeline code objects include the hash and full dump of the pipeline code object.
    // Request Format:
    //      uri requests:
    //          pipeline://getPipelines [exclusionFlags]
    //          pipeline://getAllPipelines [exclusionFlags]
    //      uri arguments:
    //          exclusionFlags  - A hex-encoded bitfield indicating which types of pipelines to exclude.
    //                            See the ExclusionFlags struct for details.
    //      POST data:
    //          Zero or more serialized PipelineHash structs
    // Response Data Format:
    //      A serialized array of PipelineRecords, and the pBinary member layed out inline.
    //      This format is the same whether the consumer requests one, 20, or "all" pipelines.
    //      <For each Pipeline>
    //          [uint64]        A 128 Pipeline Hash
    //          [uint64]            cont.
    //          [uint64]        The number of bytes in the pipeline code object data, stored in little endian.
    //          [uint8]*        The pipeline code object data
    //      </Pipelines>
    // Callback Paramaters:
    //      pService            - The service passes itself for access to the Add*() callbacks.
    //      pUserData           - The Driver's user data pointer, initialized with the Service.
    //      flags               - Exclusion flags to limit which pipelines are returned.
    //      pPipelineHashes     - An array of PipelineHash structs for which code objects are requested.
    //                              If this parameter is null and numHashes is 0, the callback should act as though
    //                              every hash was requested, except those excluded through `flags`.
    //      numHashes           - The number of pipeline hashes pointed to by pPipelineHashes.
    // Return Values:
    //      Result::Success     - if any of the requested pipelines were successfully found.
    //      Otherwise, Driver defined Result values.
    typedef DevDriver::Result (GetPipelineCodeObjects)(PipelineUriService* pService,
                                                       void*               pUserData,
                                                       ExclusionFlags      flags,
                                                       const PipelineHash* pPipelineHashes,
                                                       size_t              numHashes);

    // Overview:
    //      Clients provide an implementation of this function if they wish to support injecting pipeline code objects.
    //      Pipeline code objects include the hash and full pipeline code object binary.
    // Request Format:
    //      uri request:
    //          pipeline://reinject
    //      uri arguments:
    //          None
    //      POST data:
    //          Zero or more serialized PipelineRecord structs, with the binary code object directly following the header.
    //          <For each PipelineRecord>
    //              [uint64]        A 128 Pipeline Hash
    //              [uint64]            cont.
    //              [uint64]        The number of bytes in the pipeline code object data, stored in little endian.
    //              [uint8]*        The pipeline code object data
    //          </PipelineRecords>
    // Response Data Format:
    //      No data response
    // Callback Paramaters:
    //      pService            - The service passes itself for access to the Add*() callbacks.
    //      pUserData           - The Driver's user data pointer, initialized with the Service.
    //      pipelineIterator    - An iterator over the PipelineRecords requested for reinjection.
    // Return Values:
    //      Result::Success     - if any of the requested pipelines were successfully reinjected.
    //      Otherwise, Driver defined Result values.
    typedef DevDriver::Result (InjectPipelineCodeObjects)(void*                     pUserData,
                                                          PipelineRecordsIterator&  pipelineIterator);

    // ===== IService Methods =========================================================================================

    // Configuration information from the Driver.
    struct DriverInfo
    {
        void*                      pUserData;                    //< Driver pointer passed to the Driver's callbacks.
        GetPipelineHashes*         pfnGetPipelineHashes;         //< Driver callback to implement pipeline://index
        GetPipelineCodeObjects*    pfnGetPipelineCodeObjects;    //< Driver callback to implement pipeline://getPipelines
        InjectPipelineCodeObjects* pfnInjectPipelineCodeObjects; //< Driver callback to implement pipeline://reinject

        // Limit on the size in bytes of post blocks handled. These are used by the URI protocol to prevent DDOSing.
        // If they are set to zero, only inline post blocks are allowed.
        uint32                     postSizeLimit;                //< Post size limit for Uri requests
    };

    explicit PipelineUriService(const AllocCb& allocCb);
    virtual ~PipelineUriService();

    // Initializes the service with the Driver callbacks.
    // The service must be recreated to update the callbacks or user data.
    Result Init(const DriverInfo& info);

    // Handles a request from a consumer.
    DevDriver::Result HandleRequest(DevDriver::IURIRequestContext* pContext) override;

    // Report a size limit for post data from consumers. The limits set here are configured by the Driver.
    size_t QueryPostSizeLimit(char* pArgs) const override;

    // Returns the name of the service.
    const char*        GetName()    const override final { return kPipelineUriServiceName; }

    // Returns the version of the service.
    DevDriver::Version GetVersion() const override final { return kPipelineUriServiceVersion; }

private:
    DD_DISALLOW_COPY_AND_ASSIGN(PipelineUriService);
    DD_DISALLOW_DEFAULT_CTOR(PipelineUriService);

    AllocCb      m_allocCb;
    IByteWriter* m_pWriter;
    DriverInfo   m_driverInfo;
};

} // DevDriver
