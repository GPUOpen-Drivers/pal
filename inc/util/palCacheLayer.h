/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  palCacheLayer.h
* @brief PAL cache-utility library cache layer common interface declaraion.
***********************************************************************************************************************
*/

#pragma once

#include "palUtil.h"
#include "palMetroHash.h"
#include "palHashSet.h"
#include "palSysMemory.h"

namespace Pal
{
class IDevice;
class IPlatform;
}

namespace Util
{
/// Common declaration for an expected 128-bit hash value
using Hash128 = MetroHash::Hash;

class ICacheLayer;
class IArchiveFile;
class IPlatformKey;

/**
***********************************************************************************************************************
* @brief Lookup result from ICacheLayer::Query, entryId is specific to the cache layer it was queried from
***********************************************************************************************************************
*/
struct QueryResult
{
    ICacheLayer*    pLayer;     ///< Pointer to the layer that responded to the query
    Hash128         hashId;     ///< HashId referenced during query
    size_t          dataSize;   ///< Size of value stored in cache
    union
    {
        uint64      entryId;    ///< Unique entry id corresponding to found result
        void*       pEntryInfo; ///< Private pointer to entry data corresponding to found result
    } context;
};

/**
***********************************************************************************************************************
* @brief Common cache layer interface. Allows all cache layers to be interfaced with agnostically
***********************************************************************************************************************
*/
class ICacheLayer
{
public:
    /// Query for data by hash key
    ///
    /// @param [in]  pHashId    128-bit precomputed hash used as a reference id for the cache entry
    /// @param [out] pQuery     Query result containing the entry id and buffer size needed to call ICacheLayer::Load()
    ///
    /// @return Success if the hash id was found. Otherwise, one of the following may be returned:
    ///         + NotFound if no values was found for the given hash
    ///         + ErrorInvalidPointer if pQuery or pHashId are nullptr
    ///         + ErrorUnknown if there is an internal error.
    virtual Result Query(
        const Hash128*  pHashId,
        QueryResult*    pQuery) = 0;

    /// Store data with corresponding hash key
    ///
    /// @param [in] pHashId     128-bit precomputed hash used as a reference id for the cache entry
    /// @param [in] pData       Data to be stored in the cache
    /// @param [in] dataSize    Size of data to be stored
    ///
    /// @return Success if the data was stored under the hash id. Otherwise, one of the following may be returned:
    ///         + AlreadyExists if a value already exists for the given hash ID. Previous data will not be overwritten.
    ///         + Unsupported if the cache is read-only.
    ///         + ErrorInvalidPointer if pData or pHashId are nullptr.
    ///         + ErrorUnknown if there is an internal error.
    virtual Result Store(
        const Hash128*  pHashId,
        const void*     pData,
        size_t          dataSize) = 0;

    /// Load data from cache to buffer by entry id retrieved from Query()
    ///
    /// @param [in]  pQuery     Result returned from ICacheLayer::Query()
    /// @param [out] pBuffer    Buffer to place data from cache
    ///
    /// @return Success if data was loaded to the provided buffer. Otherwise, one of the following may be returned:
    ///         + NotFound if entryId is not present in cache
    ///         + ErrorInvalidPointer if pBuffer is nullptr
    ///         + ErrorUnknown if there is an internal error.
    virtual Result Load(
        const QueryResult* pQuery,
        void*              pBuffer) = 0;

    // Link Policy bitfield. Provides a hint as to how this layer should interact with the next
    enum LinkPolicy : uint32
    {
        // Common flags
        PassData    = 0x1ULL << 0,  ///< Data should be passed to (or read from) the next layer
        PassCalls   = 0x1ULL << 1,  ///< Function calls should be passed to the next layer
        Skip        = 0x1ULL << 2,  ///< Load/Store Operations should skip this layer

        // Store flags
        BatchStore  = 0x1ULL << 10, ///< Delay passing data to the next layer and batch for later

        // Load flags
        LoadOnQuery = 0x1ULL << 16  ///< Load data from the next layer at query time rather than load
    };

    /// Link one cache layer on top of another, does not transfer ownership of the object
    ///
    /// @param [in] pNextLayer      ICacheLayer object to become the next layer under this layer
    ///
    /// @return Success if the cache layers were linked together. Otherwise, one of the following may be returned:
    ///         + ErrorInvalidPointer if pNextLayer is nullptr
    ///         + ErrorUnknown if there is an internal error.
    virtual Result Link(
        ICacheLayer* pNextLayer) = 0;

    /// Set the layer's link policy for loading
    ///
    /// @param [in] loadPolicy      LinkPolicy flags indicating preferred behaviour on Query/Load where available
    ///
    /// @return Success if the load policy was applied. Otherwise, one of the following may be returned:
    ///         + ErrorInvalidValue if an invalid policy is provided
    ///         + ErrorUnknown if there is an internal error.
    virtual Result SetLoadPolicy(
        uint32 loadPolicy) = 0;

    /// Set the layer's link policy for storing
    ///
    /// @param [in] storePolicy     LinkPolicy flags indicating preferred behaviour on Store where available
    ///
    /// @return Success if the load policy was applied. Otherwise, one of the following may be returned:
    ///         + ErrorInvalidValue if an invalid policy is provided
    ///         + ErrorUnknown if there is an internal error.
    virtual Result SetStorePolicy(
        uint32 storePolicy) = 0;

    /// Retrieve the layer beneath this layer
    ///
    /// @return Pointer to the layer linked beneath this layer. Nullptr if layer is not linked.
    virtual ICacheLayer* GetNextLayer() const = 0;

    /// Retrieve the layer's link policy for loading
    ///
    /// @return Link policy used during Load().
    virtual uint32 GetLoadPolicy() const = 0;

    /// Retrieve the layer's link policy for storing
    ///
    /// @return Link policy used during Store().
    virtual uint32 GetStorePolicy() const = 0;

    /// Destroy Cache Layer
    virtual void Destroy() = 0;

protected:
    ICacheLayer() {}
    virtual ~ICacheLayer() {}

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ICacheLayer);
};

/**
***********************************************************************************************************************
* @brief Common cache layer construction information
***********************************************************************************************************************
*/
struct CacheLayerBaseCreateInfo
{
    AllocCallbacks* pCallbacks; ///< Memory allocation callbacks to be used by the caching layer for all long term
                                ///  storage. Allocation callbacks must be valid for the life of the cache layer
};

/**
***********************************************************************************************************************
* @brief Information needed to create an in-memory key-value store
***********************************************************************************************************************
*/
struct MemoryCacheCreateInfo
{
    CacheLayerBaseCreateInfo baseInfo;        ///< Base cache layer creation info
    size_t                   maxObjectCount;  ///< Maximum number of entries in cache
    size_t                   maxMemorySize;   ///< Maximum total size of entries in cache
    bool                     evictOnFull;     ///< Whether or not the cache should evict entries based on LRU to
                                              ///  make room for new ones
    bool                     evictDuplicates; ///< Whether or not the cache should evict entries with a duplicate hash
};

/// Get the memory size for a in-memory cache layer
///
/// @param [in]     pCreateInfo     Information about cache being created
///
/// @return Minimum size of memory buffer needed to pass to CreateMemoryCacheLayer()
size_t GetMemoryCacheLayerSize(
    const MemoryCacheCreateInfo* pCreateInfo);

/// Create an in-memory key-value caching layer
///
/// @param [in]     pCreateInfo     Information about cache being created
/// @param [in]     pPlacementAddr  Pointer to the location where the interface should be constructed. There must
///                                 be as much size available here as reported by calling GetMemoryCacheLayerSize().
/// @param [out]    ppCacheLayer    Cache layer interface. On failure this value will be set to nullptr.
///
/// @returns Success if the cache layer was created. Otherwise, one of the following errors may be returned:
///         + ErrorUnknown if there is an internal error.
Result CreateMemoryCacheLayer(
    const MemoryCacheCreateInfo* pCreateInfo,
    void*                        pPlacementAddr,
    ICacheLayer**                ppCacheLayer);

/// Get memoryCache entries number and total cache data size
///
/// @param [in]         pCacheLayer  memory cache layer to be serialized.
/// @param [out]        pCurCount    return entries number in memoryCache.
/// @param [out]        pCurSize     return total cache data size in memoryCache.
///
/// @return Success if serialize is successful.
Result GetMemoryCacheLayerCurSize(
    ICacheLayer*    pCacheLayer,
    size_t*         pCurCount,
    size_t*         pCurSize);

/// Get all hashIds in entries in memoryCache
///
/// @param [in]         pCacheLayer  memory cache layer to be serialized.
/// @param [in]         curCount    return entries number in memoryCache.
/// @param [out]        pHashIds    return total cache HashIds in memoryCache.
///
/// @return Success if serialize is successful.
Result GetMemoryCacheLayerHashIds(
    ICacheLayer*    pCacheLayer,
    size_t          curCount,
    Hash128*        pHashIds);

/**
***********************************************************************************************************************
* @brief Information needed to create an archive file backed key-value store
***********************************************************************************************************************
*/
struct ArchiveFileCacheCreateInfo
{
    CacheLayerBaseCreateInfo baseInfo;     ///< Base cache layer creation info.
    IArchiveFile*            pFile;        ///< Archive file to use for storage, must exist for the lifetime of the
                                           ///  cache layer. May be shared between multiple layers but no internal
                                           ///  thread safety is provided.
    const IPlatformKey*      pPlatformKey; ///< Optional platform key, allows for data stored to the archive file
                                           ///  to be keyed to a specific driver/platform fingerprint.
    uint32                   dataTypeId;   ///< Optional 32-bit data type identifier, allows heterogenous data to be
                                           ///  stored within an archive file.
};

/// Get the memory size for a archive file backed cache layer
///
/// @param [in]     pCreateInfo     Information about cache being created
///
/// @return Minimum size of memory buffer needed to pass to CreateArchiveFileCacheLayer()
size_t GetArchiveFileCacheLayerSize(
    const ArchiveFileCacheCreateInfo* pCreateInfo);

/// Create an archive file backed caching layer
///
/// @param [in]     pCreateInfo     Information about cache being created
/// @param [in]     pPlacementAddr  Pointer to the location where the interface should be constructed. There must
///                                 be as much size available here as reported by calling
///                                 GetArchiveFileCacheLayerSize().
/// @param [out]    ppCacheLayer    Cache layer interface. On failure this value will be set to nullptr.
///
/// @returns Success if the cache layer was created. Otherwise, one of the following errors may be returned:
///         + ErrorUnknown if there is an internal error.
Result CreateArchiveFileCacheLayer(
    const ArchiveFileCacheCreateInfo* pCreateInfo,
    void*                             pPlacementAddr,
    ICacheLayer**                     ppCacheLayer);

/**
***********************************************************************************************************************
* @brief Information needed to create a pipeline content tracker
***********************************************************************************************************************
*/
struct TrackingCacheCreateInfo
{
    AllocCallbacks* pCallbacks; ///< Memory allocation callbacks to be used by the caching layer for all long term
                                ///  storage. Allocation callbacks must be valid for the life of the cache layer
};

using TrackedHashSet = HashSet<
    Hash128,
    ForwardAllocator>;

using TrackedHashIter = TrackedHashSet::Iterator;

using GetTrackedHashes = TrackedHashIter(*)(const ICacheLayer*);

/// Get the memory size for a pipeline tracking cache layer
///
/// @return Minimum size of memory buffer needed to pass to CreateTrackingCacheLayer()
size_t GetTrackingCacheLayerSize();

/// Create a pipeline tracking cache layer
///
/// @param [in]     pCreateInfo         Information about cache being created
/// @param [in]     pPlacementAddr      Pointer to the location where the interface should be constructed. There
///                                     must be as much size available here as reported by calling
///                                     GetTrackingCacheLayerSize().
/// @param [out]    ppCacheLayer        Cache layer interface. On failure this value will be set to nullptr.
/// @param [out]    ppGetTrackedHashes  Function pointer for entry retrieval. On failure this value will be set to
///                                     nullptr.
///
/// @returns Success if the cache layer was created. Otherwise, one of the following errors may be returned:
///         + ErrorUnknown if there is an internal error.
Result CreateTrackingCacheLayer(
    const TrackingCacheCreateInfo*  pCreateInfo,
    void*                           pPlacementAddr,
    ICacheLayer**                   ppCacheLayer,
    GetTrackedHashes*               ppGetTrackedHashes);

} // namespace Util
