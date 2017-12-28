/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palShaderCache.h
 * @brief Defines the Platform Abstraction Library (PAL) IShaderCache interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palDestroyable.h"
#include "palPipeline.h"

namespace Pal
{

/// Defines callback function used to lookup shader cache info in an external cache
typedef Result (PAL_STDCALL *ShaderCacheGetValue)
    (const void* pClientData,
    ShaderHash hash,
    void* pValue, size_t* pValueLen);

/// Defines callback function used to store shader cache info in an external cache
typedef Result (PAL_STDCALL *ShaderCacheStoreValue)
    (const void* pClientData,
    ShaderHash hash,
    const void* pValue, size_t valueLen);

/// Specifies all information necessary to create a shader cache object.
struct ShaderCacheCreateInfo
{
    const void*            pInitialData;       ///< Pointer to the data that should be used to initalize the shader
                                               ///  cache. This pointer may be null if no intial data is available.
    size_t                 initialDataSize;    ///< Size of the intial data pointed to by pInitialData.
    // The following parameters are optional and are only used when the IShaderCache will be used with an external cache
    // for storage of the compiled shader data.  If these parameters are populated then the shader cache will utilize
    // the functions to search for and store compiled shader data instead of using it's own internal data structure.
    // If the parameters are left null then the shader cache will ignore them and use it's own storage.
    ShaderCacheGetValue    pfnGetValueFunc;    ///< [Optional] Function to lookup shader cache data in an external cache
    ShaderCacheStoreValue  pfnStoreValueFunc;  ///< [Optional] Function to store shader cache data in an external cache

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 316
    uint32                 expectedEntries;    ///< [optional] The number of entries expected to be inserted into this
                                               ///  cache. Leaving it 0 means let PAL choose default value.
#endif
};

/**
 ***********************************************************************************************************************
 * @interface IShaderCache
 * @brief     This class implements a cache for compiled shaders. The shader cache is designed to be optionally passed
 *            in at Pipeline create time. The compiled binary for the shaders is stored in the cache object to avoid
 *            compiling the same shader multiple times. The shader cache also provides a method to serialize its data
 *            to be stored to disk.
 *
 * @see IDevice::CreateShaderCache()
 ***********************************************************************************************************************
 */
class IShaderCache : public IDestroyable
{
public:
    /// Serializes the shader cache data or queries the size required for serialization.
    ///
    /// @param [in,out]  pBlob System memory pointer where the serialized data should be placed.  This parameter can be
    ///                        nullptr when querying the size of the serialized data. When non-null (and the size is
    ///                        correct/sufficient) then the contents of the shader cache will be placed in this
    ///                        location.  The data is an opaque blob which is not intended to be parsed by clients.
    /// @param [in,out]  pSize Size of the memory pointed to by pBlob.  If the value stored in pSize is zero then
    ///                        no data will be copied and instead the size required for serialization will be returned
    ///                        in pSize.
    ///
    /// @returns Success if data was serialized successfully, NotReady if the size was returned, or Unavailable if the
    ///          shader cache is unintialized/invalid.
    virtual Result Serialize(
        void*   pBlob,
        size_t* pSize) = 0;

    /// Resets the runtime shader cache to an empty state. Releases all internal allocator memory back to the OS.
    virtual void Reset() = 0;

    /// Merges the provided source shader caches' content into this shader cache.
    ///
    /// @param [in]  numSrcCaches Number of source shader caches to be merged.
    /// @param [in]  ppSrcCaches  Pointer to an array of pointers to IShaderCache.
    ///
    /// @returns Success if data of source shader caches was merged successfully, OutOfMemory if the internal allocator
    ///          memory cannot be allocated.
    virtual Result Merge(
        uint32               numSrcCaches,
        const IShaderCache** ppSrcCaches) = 0;

    /// Returns the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @returns Pointer to client data.
    PAL_INLINE void* GetClientData() const { return m_pClientData; }

    /// Sets the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @param  [in]    pClientData     A pointer to arbitrary client data.
    PAL_INLINE void SetClientData(
        void* pClientData) { m_pClientData = pClientData; }

protected:
    /// @internal Constructor. Prevent use of new operator on this interface. Client must create objects by explicitly
    /// called the proper create method.
    IShaderCache() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IShaderCache() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
