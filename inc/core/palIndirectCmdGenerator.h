/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palIndirectCmdGenerator.h
 * @brief Defines the Platform Abstraction Library (PAL) IIndirectCmdGenerator interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palGpuMemoryBindable.h"

namespace Pal
{

/// Enumerates the different types of command parameters which can be translated by an indirect command generator.
enum class IndirectParamType : uint32
{
    Draw = 0,       ///< Initiates a non-index draw operation.  The contents of the arguments buffer must contain a
                    ///  @ref DrawIndirectArgs structure.  This must be the last command parameter.
    DrawIndexed,    ///< Initiates an indexed draw operation. The contents of the arguments buffer must contain a
                    ///  @ref DrawIndexedIndirectArgs structure.  This must be the last command parameter.
    Dispatch,       ///< Initiates a dispatch operation.  The contents of the arguments buffer must contain a
                    ///  @ref DispatchIndirectArgs structure.  This must be the last command parameter.
    DispatchMesh,   ///< Initiates a dispatch mesh operation.  The contents of the arguments buffer must contain a
                    ///  @ref DispatchMeshIndirectArgs structure.  This must be the last command parameter.
    BindIndexData,  ///< Binds a range of GPU memory for use as an index buffer.  This parameter is only allowed if
                    ///  a DrawIndex parameter is also present, and can only appear once per command generator.
    BindVertexData, ///< Binds a range of GPU memory for use as a vertex buffer.  This parameter is not allowed if
                    ///  Dispatch parameter is also present.
    SetUserData,    ///< Sets one or more user-data entries.
};

/// Specifies the layout in GPU memory used to represent a 'BindIndexData' indirect command parameter.
struct BindIndexDataIndirectArgs
{
    gpusize  gpuVirtAddr;   ///< Starting GPU virtual address of the index data, in bytes.  Must be aligned to the
                            ///  index element size.
    uint32   sizeInBytes;   ///< Size, in bytes, of the index data.  Must be aligned to the index element size.
    uint32   format;        ///< Format token indicating which type of index buffer is being bound.
};

/// Specifies the layout in GPU memory used to represent a 'BindVertexData' indirect command parameter.
struct BindVertexDataIndirectArgs
{
    gpusize  gpuVirtAddr;    ///< Starting GPU virtual address of the buffer, in bytes.  Must be aligned to a multiple
                             ///  of strideInBytes.
    uint32   sizeInBytes;    ///< Size, in bytes, of the buffer. Must be a multiple of strideInBytes, except when
                             ///  strideInBytes is zero.
    uint32   strideInBytes;  ///< Per-record stride of the buffer.  See @ref BufferViewInfo for more information about
                             ///  setting-up untyped buffer SRD's.
};

/// Contains all information about a single indirect command parameter.
struct IndirectParam
{
    IndirectParamType  type;        ///< Type of indirect command parameter this is.

    /// Size, in bytes, of the data representing this command parameter, as stored in an indirect arguments buffer.  The
    /// type of parameter indicates the legal sizes of the parameter in GPU memory.
    ///
    /// Draw           |  Must equal sizeof(DrawIndirectArgs).
    /// DrawIndexed    |  Must equal sizeof(DrawIndexedIndirectArgs).
    /// Dispatch       |  Must equal sizeof(DispatchIndirectArgs).
    /// DispatchMesh   |  Must equal sizeof(DispatchMeshIndirectArgs).
    /// BindIndexData  |  Must equal sizeof(BindIndexDataIndirectArgs).
    /// BindVertexData |  Must equal sizeof(BindVertexDataIndirectArgs).
    /// SetUserData    |  Must equal (sizeof(uint32) * userData.entryCount).
    uint32  sizeInBytes;

    /// Shader usage mask defining which API shader stages access a IndirectParamType::SetUserData (@see
    /// ShaderStageFlagBits). indirect parameter.  Must be ApiShaderStageCompute for IndirectParamType::Dispatch.
    uint32  userDataShaderUsage;

    union
    {
        struct
        {
            uint32  firstEntry;     ///< First user-data entry to set.
            uint32  entryCount;     ///< Number of user-data entries to set.
        } userData;                 ///< Additional information about a 'SetUserData' indirect command parameter.

        struct
        {
            uint32  bufferId;       ///< Vertex bufer slot ID to set.
        } vertexData;               ///< Additional information about a 'BindVertexData' indirect command parameter.
    };
};

/// Specifies the information needed to create an indirect command generator object.
struct IndirectCmdGeneratorCreateInfo
{
    /// Array of indirect command parameters which describe the layout of the indirect arguments buffer to the
    /// command generator.  Every command generated by the generator has the same layout in GPU memory.
    const IndirectParam*  pParams;
    uint32                paramCount;   ///< Number of IndirectParam structures pointed-to by pParams.  This must be
                                        ///  at least one.

    /// Stride, in bytes, of each indirect command stored in the client's indirect arguments buffer.  This must be
    /// at least as large as the size of all command parameters stored sequentially (i.e., there can be zero or more
    /// bytes of padding between indirect commands).
    uint32  strideInBytes;

    /// Set of magic values which the command generator will recognize inside a BindIndexDataIndirectArgs structure
    /// to choose an index-buffer type: [0] = 8 bit indices, [1] = 16 bit indices, [2] = 32 bit indices.
    uint32  indexTypeTokens[3];
};

/**
 ***********************************************************************************************************************
 * @interface IIndirectCmdGenerator
 * @brief     Translates an application-specified pseudo command buffer into a format compatible with AMD GPU's.
 *
 * An indirect command generator is used to permit client applications to generate their own "command buffers" using
 * the GPU.  The client's pseudo command buffers must adhere to a format which is illustrated by the structures listed
 * in this header.  This interface describes an object which is capable of taking these pseudo-commands and generating
 * a command buffer which can be executed on an AMD GPU.
 *
 * This feature essentially allows the client to support a more-flexible version of DrawIndirect which allows changing
 * the index buffer binding and/or user data entries between draws or dispatches.
 *
 * @see IDevice::CreateIndirectCmdGenerator()
 ***********************************************************************************************************************
 */
class IIndirectCmdGenerator : public IGpuMemoryBindable
{
public:

    /// Returns the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @returns Pointer to client data.
    void* GetClientData() const
    {
        return m_pClientData;
    }

    /// Sets the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @param  [in]    pClientData     A pointer to arbitrary client data.
    void SetClientData(
        void* pClientData)
    {
        m_pClientData = pClientData;
    }

protected:
    /// @internal Constructor. Prevent use of new operator on this interface. Client must create objects by explicitly
    /// called the proper create method.
    IIndirectCmdGenerator() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IIndirectCmdGenerator() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;

};

} // Pal
