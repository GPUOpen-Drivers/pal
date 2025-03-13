/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "pal.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"

namespace Util { namespace PalAbi { struct PipelineMetadata; } }

namespace Util { namespace HsaAbi { class CodeObjectMetadata; } }

namespace Pal
{

class Device;

namespace Gfx12
{

constexpr uint16 NoUserDataSpilling = static_cast<uint16>(USHRT_MAX);

struct GfxState;

union UserDataReg
{
    struct
    {
        uint32 regOffset : 10;
        uint32           : 22;
    };
    uint32 u32All;
};

union MultiUserDataReg
{
    struct
    {
        uint32 regOffset0 : 10;
        uint32 regOffset1 : 10;
        uint32 regOffset2 : 10;
        uint32            :  2;
    };
    uint32 u32All;
};

// =====================================================================================================================
// Common base class for common elements of Graphics/Compute user data layouts.
class UserDataLayout
{
public:
    void Destroy();

    uint32 GetSpillThreshold() const { return m_spillThreshold; }

    uint32 GetNumMapWords() const { return m_numMapWords; }

    const uint32* GetMapping() const { return m_pMap; }

protected:
    UserDataLayout(const Pal::Device& device, uint32* pMap, uint32 numMapWords, uint32 spillThreshold);
    virtual ~UserDataLayout() { }

    // Struct describing what Gfx12 state needs to be updated when binding this layout after another.
    struct LayoutDelta
    {
        uint32 firstStaleMapWord; // First word in m_pMap that must be re-sent to the HW.
        uint32 numStaleMapWords;  // Number of words to be re-copied from m_pMap.
        uint32 firstStaleEntry;   // First user data entry that must be re-sent to the HW due to a new mapping.
        uint32 numStaleEntries;   // Number of user data entries to be re-sent to the HW.
    };

    // Examines a prior bound user data layout vs. the current to minimize the amount of state that needs to be
    // re-sent to the HW.  Returns true if any delta is found, false if they are identical.
    bool ComputeLayoutDelta(const UserDataLayout* pPrevLayout, LayoutDelta* pOut) const;

    const Pal::Device& m_device;

    // Hash to identify objects owned by separate pipelines that actually define an identical mapping.
    uint64 m_hash;

    // Table mapping virtual user data entries to physical user data registers.  m_numMapWords is the size of that
    // table.  The exact contents vary slightly between compute/graphics.
    uint32*      m_pMap;
    const uint32 m_numMapWords;

    // m_spillThreshold specifies the first virtual user data entry (i.e., shader input passed in via CmdSetUserData())
    // that cannot fit in physical user data registers for all relevant stages; user data entries at this point and
    // higher must be spilled to memory.
    const uint32 m_spillThreshold;
};

// =====================================================================================================================
// Defines mapping of virtual user data entries and other state (draw params, VB tables, etc.) to physical user data
// registers and spill memory.
class GraphicsUserDataLayout final : public UserDataLayout
{
public:
    static Result Create(
        const Pal::Device&                    device,
        const Util::PalAbi::PipelineMetadata& metadata,
        GraphicsUserDataLayout**              ppObject);

    static Result Create(
        const Pal::Device&              device,
        const GraphicsUserDataLayout&   preRasterLayout,
        const GraphicsUserDataLayout&   psLayer,
        GraphicsUserDataLayout**        ppObject);

    template <bool PipelineSwitch>
    uint32* CopyUserDataPairsToCmdSpace(
        const GraphicsUserDataLayout* pPrevGfxUserDataLayout,
        const UserDataFlags&          dirty,
        const uint32*                 pUserData,
        uint32*                       pCmdSpace) const;

    bool             ViewInstancingEnable()      const { return m_viewId.u32All != 0;     }
    uint32           EsGsLdsSizeRegOffset()      const { return m_esGsLdsSize.regOffset;  }
    UserDataReg      GetVertexBufferTable()      const { return m_vertexBufferTable;      }
    MultiUserDataReg GetSpillTable()             const { return m_spillTable;             }
    uint32           GetUserDataLimit()          const { return m_userDataLimit;          }
    UserDataReg      GetVertexBase()             const { return m_baseVertex;             }
    UserDataReg      GetInstanceBase()           const { return m_baseInstance;           }
    UserDataReg      GetDrawIndex()              const { return m_drawIndex;              }
    UserDataReg      GetMeshDispatchDims()       const { return m_meshDispatchDims;       }
    MultiUserDataReg GetViewId()                 const { return m_viewId;                 }
    UserDataReg      GetStreamoutTable()         const { return m_streamoutTable;         }
    UserDataReg      GetStreamoutCtrlBuf()       const { return m_streamoutCtrlBuf;       }
    UserDataReg      GetMeshRingIndex()          const { return m_meshRingIndex;          }
    UserDataReg      GetSampleInfo()             const { return m_sampleInfo;             }
    UserDataReg      GetColorExportAddr()        const { return m_colorExportAddr;        }
    UserDataReg      GetPrimNeededCnt()          const { return m_primsNeededCnt;         }
    UserDataReg      GetNggCullingData()         const { return m_nggCullingData;         }
    MultiUserDataReg GetCompositeData()          const { return m_compositeData;          }

    Result Duplicate(
        const Pal::Device&       device,
        GraphicsUserDataLayout** ppOther) const;

private:
    // Internal create info struct with values translated by the static Create() function from the ABI metadata.
    struct CreateInfo
    {
        UserDataReg             baseVertex;
        UserDataReg             baseInstance;
        UserDataReg             drawIndex;
        UserDataReg             vertexBufferTable;
        UserDataReg             streamoutCtrlBuf;
        UserDataReg             streamoutTable;
        UserDataReg             esGsLdsSize;
        UserDataReg             meshDispatchDims;
        UserDataReg             meshRingIndex;
        UserDataReg             sampleInfo;
        UserDataReg             colorExportAddr;
        UserDataReg             primsNeededCnt;
        UserDataReg             nggCullingData;
        MultiUserDataReg        viewId;
        MultiUserDataReg        compositeData;
        uint32                  spillThreshold;
        MultiUserDataReg        spillTable;
        uint32                  numMapWords;
        const MultiUserDataReg* pMap;
        uint32                  userDataLimit;
    };

    explicit GraphicsUserDataLayout(const Pal::Device& device, const CreateInfo& createInfo);
    virtual ~GraphicsUserDataLayout() { }

    static size_t Size(const CreateInfo& createInfo);

    // The following values are system generated values that can map to either 0 or 1 physical user data register.
    const UserDataReg      m_baseVertex;
    const UserDataReg      m_baseInstance;
    const UserDataReg      m_drawIndex;
    const UserDataReg      m_vertexBufferTable;
    const UserDataReg      m_streamoutCtrlBuf;
    const UserDataReg      m_streamoutTable;
    const UserDataReg      m_esGsLdsSize;
    const UserDataReg      m_meshDispatchDims;
    const UserDataReg      m_meshRingIndex;
    const UserDataReg      m_sampleInfo;
    const UserDataReg      m_colorExportAddr;
    const UserDataReg      m_primsNeededCnt;
    const UserDataReg      m_nggCullingData;
    const MultiUserDataReg m_viewId;
    const MultiUserDataReg m_compositeData;

    // Defines the physical user data register(s) that must be updated with a pointer to a new spill table each time a
    // user data entry at or above the m_spillThreshold is updated.
    const MultiUserDataReg m_spillTable;

    const uint32           m_userDataLimit;

    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsUserDataLayout);
};

// Defines creation parameters for a ComputeUserDataLayout object.
struct ComputeUserDataLayoutCreateInfo
{
    UserDataReg        workgroup;
    uint32             spillThreshold;
    UserDataReg        spillTable;
    UserDataReg        meshTaskDispatchDims;
    UserDataReg        meshTaskRingIndex;
    UserDataReg        taskDispatchIndex;
    uint32             numMapWords;
    const UserDataReg* pMap;
    uint32             userDataLimit;
};

// =====================================================================================================================
// Defines mapping of virtual user data entries and other state (workgroup ID, spill table ptr, etc.) to to physical
// user data registers and spill memory.
class ComputeUserDataLayout final : public UserDataLayout
{
public:
    static Result Create(
        const Pal::Device&                    device,
        const Util::PalAbi::PipelineMetadata& metadata,
        ComputeUserDataLayout**               ppObject);

      static Result Create(
          const Pal::Device&                      device,
          const Util::HsaAbi::CodeObjectMetadata& metadata,
          ComputeUserDataLayout**                 ppObject);

      template <bool PipelineSwitch>
    uint32* CopyUserDataPairsToCmdSpace(
        const ComputeUserDataLayout* pPrevComputeUserDataLayout,
        const Pal::UserDataFlags&    dirty,
        const uint32*                pUserData,
        uint32*                      pCmdSpace) const;

    UserDataReg GetSpillTable()        const { return m_spillTable;        }
    UserDataReg GetWorkgroup()         const { return m_workgroup;         }
    UserDataReg GetTaskDispatchDims()  const { return m_taskDispatchDims;  }
    UserDataReg GetMeshTaskRingIndex() const { return m_meshTaskRingIndex; }
    UserDataReg GetTaskDispatchIndex() const { return m_taskDispatchIndex; }
    uint32      GetUserDataLimit()     const { return m_userDataLimit;     }

    Result Duplicate(
        const Pal::Device&      device,
        ComputeUserDataLayout** ppOther) const;

    Result CombineWith(
        const Pal::Device&      device,
        ComputeUserDataLayout** ppOther) const;

private:
    explicit ComputeUserDataLayout(const Pal::Device& device, const ComputeUserDataLayoutCreateInfo& createInfo);
    virtual ~ComputeUserDataLayout() { }

    static size_t Size(const ComputeUserDataLayoutCreateInfo& createInfo);

    // The following values are system generated values that can map to either 0 or 1 physical user data register.
    const UserDataReg m_workgroup;

    // The following values define offsets of the physical user data register for task shader.
    const UserDataReg m_taskDispatchDims;
    const UserDataReg m_meshTaskRingIndex;
    const UserDataReg m_taskDispatchIndex;

    // m_spillTable defines the physical user data register that must be updated with a pointer to a new spill table
    // each time a user data entry at or above the m_spillThreshold is updated.
    const UserDataReg m_spillTable;

    const uint32      m_userDataLimit;

    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeUserDataLayout);
};

} // namespace Gfx12
} // namespace Pal
