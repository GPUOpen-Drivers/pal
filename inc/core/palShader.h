/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palShader.h
 * @brief Defines the Platform Abstraction Library (PAL) IShader interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palDestroyable.h"

/// The major version of AMD IL that PAL can parse correctly. Shaders compiled with a larger major version may not be
/// parsed appropriately.
#define PAL_SUPPORTED_IL_MAJOR_VERSION 2

namespace Pal
{
/// Number of SC_COPT bitfield array uint32s
constexpr uint32 ScOptionsDwords = 7;

/// Flags controlling shader creation.
union ShaderCreateFlags
{
    struct
    {
        uint32 allowReZ       :  1; ///< Allow the DB ReZ feature to be enabled.  This will cause an early Z test
                                    ///  to potentially kill PS waves before launch, and also perform a late Z
                                    ///  test/write in case the PS kills pixels.  Only valid for pixel shaders.
        uint32 outputWinCoord :  1; ///< Vs output window position.
        uint32 reserved       : 30; ///< Reserved for future use.
    };
    uint32 u32All;                  ///< Flags packed as 32-bit uint.
};

/// Flags controlling strategies the shader compiler should take to minimize VGPR usage
union ShaderMinVgprStrategyFlags
{
    struct
    {
        uint32 minimizeVgprsUsageGcmseq     :  1; ///< Enable global code motion transformation.
        uint32 minimizeVgprsUsageSched      :  1; ///< Make scheduler favor minimizing VGPR usage.
        uint32 minimizeVgprsUsageRegAlloc   :  1; ///< Make register allocator favor minimizing VGPR usage.
        uint32 minimizeVgprsUsageMergeChain :  1; ///< Enable merge chaining in scheduler preprocess.
        uint32 minimizeVgprsUsagePeepHole   :  1; ///< Control peephole optimizations that can affect VGPR usage.
        uint32 minimizeVgprsUsageCubeCoord  :  1; ///< Replace cubetc/cubesc/cubema/cubeid HW instructions
                                                  ///  with a sequence of ALUs.
        uint32 minimizeVgprsUsageFactorMad  :  1; ///< A common mul will be factored from a series on mads.
        uint32 minimizeVgprsUsageVN         :  1; ///< ValueNumber optimizations will be reduced, those that
                                                  ///  have tendency to increase VGPR pressure will be disabled.
        uint32 minimizeVgprsUsageBCM        :  1; ///< Enable a 'Bulk Code Motion' phase which attempts to move
                                                  ///  instructions closer to their uses.
        uint32 reserved                     : 23; ///< Reserved for future use.
    };
    uint32 u32All;                                ///< Flags packed as 32-bit uint.
};

/// Flags controlling optimization strategies
union ShaderOptimizationStrategyFlags
{
    struct
    {
        uint32 minimizeVgprs              :  1; ///< The shader should be compiled to minimize the number of VGPRs.
        uint32 clientVgprLimit            :  1; ///< Override the maximum number of VGPRs that SC can use for this
                                                ///  shader with the value specified in the vgprLimit field.
        uint32 userDataSpill              :  1; ///< Override user data register spill threshold.
        uint32 useScGroup                 :  1; ///< Allow the SC instruction scheduler to score the 'group'
                                                ///  schedule as one of its options.
        uint32 useScLiveness              :  1; ///< SC's liveness-based instruction scheduling.
        uint32 useScRemat                 :  1; ///< Coupled with the setting of vgpr counts, the SC will
                                                ///  aggressively try to meet that value by rematerializing
                                                ///  instructions at their use site when it lowers overall
                                                ///  register pressure.
        uint32 useScUseMoreD16            :  1; ///< SC option to force textures to use D16 (GFX9-only).
        uint32 useScUseUnsafeMadMix       :  1; ///< SC option to ignore denorm issue to use more MAD_MIX.
        uint32 useScUnsafeConvertToF16    :  1; ///< SC option to attempt a potentially unsafe conversion of F32 to F16.
        uint32 removeNullParameterExports :  1; ///< If possible, null paramter exports will be optimized out of
                                                ///  the compile primitive shader.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 291
        uint32 useScAggressiveHoist       :  1; ///< SC option to disable register pressure checking when evaluating a
                                                ///  potential hoist of loop invariant.
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 293
        uint32 useScXnackEnable           :  1; ///< SC option to enable XNack support if it is not already enabled
                                                ///  due to intra-submit migration requirements.
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 294
        uint32 useNonIeeeFpInstructions   :  1; ///< Use legacy non-ieee floating point instructions
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 357
        uint32 enabledPerformanceData     :  1; ///< Enables the compiler to generate extra instructions to gather
                                                ///  various performance-related data.
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 363
        uint32 allowNonIeeeOperations     : 1;  ///< Allows SC to make optimizations at the expense of IEEE compliance.
        uint32 appendBufPerWaveAtomic     : 1;  ///< Controls whether or not shaders should execute one atomic
                                                ///  instruction per wave for UAV append/consume operations. If false,
                                                ///  one atomic will be executed per thread.
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 363
        uint32 reserved                   : 16; ///< Reserved for future use.
#elif PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 357
        uint32 reserved                   : 18; ///< Reserved for future use.
#elif PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 294
        uint32 reserved                   : 19; ///< Reserved for future use.
#elif PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 293
        uint32 reserved                   : 20; ///< Reserved for future use.
#elif PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 291
        uint32 reserved                   : 21; ///< Reserved for future use.
#else
        uint32 reserved                   : 22; ///< Reserved for future use.
#endif
    };
    uint32 u32All;                   ///< Flags packed as 32-bit uint.
};

/// Structure controlling shader optimization.
struct ShaderOptimizationStrategy
{
    ShaderOptimizationStrategyFlags flags;                  ///< Flags controlling optimization strategies.
    uint32                          vgprLimit;              ///< Max VGPR limit to give to SC for compilation for this
                                                            ///  shader. This only applies if the clientVgprLimit flag
                                                            ///  is set.
    uint32                          maxLdsSpillDwords;      ///< Max LDS size in DWORDS that is used for spilling
                                                            ///  registers.
    ShaderMinVgprStrategyFlags      minVgprStrategyFlags;   ///< Optimal MinVGPRStrategy requested by client for this
                                                            ///  shader.
    uint32                          userDataSpillThreshold; ///< Limit of user data registers PAL will use (0 means PAL
                                                            ///  will spill all client user data entries to memory)
                                                            ///  - ignored unless the userDataSpill flag is set.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 302
    uint32                          csTgPerCu;              ///< Override the number of threadgroups that a particular
                                                            ///  CS can run on, throttling it, to enable more graphics
                                                            ///  work to complete.
#endif
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 341
#if PAL_DEVELOPER_BUILD
    uint32 scOptions[ScOptionsDwords];     ///< Array of uint32s whose bitfeilds represent the constants in the
                                           ///< SC_COPT enum, e.g. scOptions[0] represents enum values 0 - 31,
                                           ///< scOptions[1] represents enum values 32- 63, etc.
    uint32 scOptionsMask[ScOptionsDwords]; ///< Array of uint32s whose bitfields mask out the corresponding
                                           ///< bits in the scOptions array to be set.
#endif
#endif
};

/// ShaderHash represents a 128-bit shader hash.
struct ShaderHash
{
    uint64 lower;   ///< Lower 64-bits of hash
    uint64 upper;   ///< Upper 64-bits of hash
};

/// Determines whether two ShaderHashes are equal.
///
/// @param  [in]    hash1    The first 128-bit shader hash
/// @param  [in]    hash2    The second 128-bit shader hash
///
/// @returns True if the shader hashes are equal.
PAL_INLINE bool ShaderHashesEqual(
    const ShaderHash hash1,
    const ShaderHash hash2)
{
    return ((hash1.lower == hash2.lower) & (hash1.upper == hash2.upper));
}

/// Determines whether the given ShaderHash is non-zero.
///
/// @param  [in]    hash    A 128-bit shader hash
///
/// @returns True if the shader hash is non-zero.
PAL_INLINE bool ShaderHashIsNonzero(
    const ShaderHash hash)
{
    return ((hash.upper | hash.lower) != 0);
}

/// Specifies properties for @ref IShader creation.  Input structure to IDevice::CreateShader().
struct ShaderCreateInfo
{
    ShaderCreateFlags           flags;         ///< Flags controlling shader creation.
    size_t                      codeSize;      ///< Input shader code size in bytes.
    const void*                 pCode;         ///< Pointer to the input shader binary AMD IL.
    ShaderHash                  clientHash;    ///< Client-supplied shader hash.  A value of 0 indicates that PAL
                                               ///  should use its own hash for dumping purposes.
    ShaderOptimizationStrategy  optStrategy;   ///< Optimization Strategy for this shader.
};

/// Specifies a shader type (i.e., what stage of the pipeline this shader was written for).
enum class ShaderType : uint32
{
    Compute = 0,
    Vertex,
    Hull,
    Domain,
    Geometry,
    Pixel,
};

/// Number of shader program types supported by PAL.
constexpr uint32 NumShaderTypes =
    (1u + static_cast<uint32>(ShaderType::Pixel) - static_cast<uint32>(ShaderType::Compute));

/// Specifies the API slot used by a Generic User Data declaration in an IL shader.
enum class GenericUserData : uint32
{
    /// Vertex shaders need to know what the starting vertex of the draw was so that they can correctly determine
    /// the vertex ID based off of the hardware's internal vertexID counter.
    StartVertex = 0,
    /// Vertex shaders also need to know the what starting instance of an instanced draw was, so they can correctly
    /// determine the instance ID based off of the hardware's internal instanceID counter.
    StartInstance = 1,
    /// When issuing a multi-draw-indirect, vertex shaders sometimes need to know which draw in the multi-draw a VS
    /// invocation belongs to. This user data entry is used to communicate that information to the shader.  This is
    /// only supported for Vulkan.
    DrawIndex = 2,
    /// Some compute kernels need to know how many thread groups were dispatched in the X, Y and Z dimensions.  This
    /// user data slot is used for communicating that information to the shader.  This is only supported for Vulkan.
    NumThreadGroupsXYZ = 3,
    /// View id identifies a view of graphic pipeline instancing. Graphic pipeline instancing is orthogonal to
    /// draw instancing. An obvious way to use graphic pipeline instancing is for the shader stage feeding
    /// the rasterizer to generate position, viewport array index or render target array index as a function of view id.
    ViewId = 4,
    /// Number of generic user data API slots that are used in an IL shader.
    Count,
};

/// Special constant buffer id @ref IShader::UsesPushConstants() call.
constexpr uint32 PushConstantBufferId = 255;

/**
 ***********************************************************************************************************************
 * @interface IShader
 * @brief     Shaders are not directly executed by the GPU, and are only used as building blocks for pipeline objects.
 *
 * @see IDevice::CreateShader()
 ***********************************************************************************************************************
 */
class IShader : public IDestroyable
{
public:
    /// Returns the shader type (e.g. Compute, Pixel) for this shader.
    ///
    /// @returns The @ref ShaderType for this particular shader.
    virtual ShaderType GetType() const = 0;

    /// Checks whether push constants are used for this particular shader. Push constant is a term of Vulkan which means
    /// a kind of constant could be accessed in a fast path instead of being accessed via normal constant buffer fetch.
    /// This can be mapped to our hardware's user data registers if there are any available. A special constant buffer
    /// 255 is used in IL and should be mapped to ResourceMappingNodeType::InlineConst.
    /// Vulkan may not need this method as it defines push constant in its pipeline create info, while Mantle does need
    /// a way to be aware of using of push constant and to be able to set up its user data entries correctly. As Mantle
    /// only needs a very limited amount of user data registers (2-4), so this method doesn't check how many constants
    /// (and channels) are used in IL. Mantle assumes there is only 1 constant.
    ///
    /// @returns True if push constant is used for this shader.
    virtual bool UsesPushConstants() const = 0;

    /// Returns the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @returns Pointer to client data.
    PAL_INLINE void* GetClientData() const
    {
        return m_pClientData;
    }

    /// Sets the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @param  [in]    pClientData     A pointer to arbitrary client data.
    PAL_INLINE void SetClientData(
        void* pClientData)
    {
        m_pClientData = pClientData;
    }

protected:
    /// @internal Constructor. Prevent use of new operator on this interface. Client must create objects by explicitly
    /// called the proper create method.
    IShader() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IShader() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
