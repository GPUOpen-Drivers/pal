/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palHsaAbiMetadata.h"
#include "palInlineFuncs.h"
#include "palMsgPackImpl.h"

namespace Util
{
namespace HsaAbi
{

namespace KernArgsMetadataKey
{
    static constexpr char Name[]         = ".name";
    static constexpr char TypeName[]     = ".type_name";
    static constexpr char Size[]         = ".size";
    static constexpr char Offset[]       = ".offset";
    static constexpr char ValueKind[]    = ".value_kind";
    static constexpr char PointeeAlign[] = ".pointee_align";
    static constexpr char AddressSpace[] = ".address_space";
    static constexpr char Access[]       = ".access";
    static constexpr char ActualAccess[] = ".actual_access";
    static constexpr char IsConst[]      = ".is_const";
    static constexpr char IsRestrict[]   = ".is_restrict";
    static constexpr char IsVolatile[]   = ".is_volatile";
    static constexpr char IsPipe[]       = ".is_pipe";
};

namespace KernelMetadataKey
{
    static constexpr char Name[]                    = ".name";
    static constexpr char Symbol[]                  = ".symbol";
    static constexpr char Language[]                = ".language";
    static constexpr char LanguageVersion[]         = ".language_version";
    static constexpr char Args[]                    = ".args";
    static constexpr char ReqdWorkgroupSize[]       = ".reqd_workgroup_size";
    static constexpr char WorkgroupSizeHint[]       = ".workgroup_size_hint";
    static constexpr char VecTypeHint[]             = ".vec_type_hint";
    static constexpr char DeviceEnqueueSymbol[]     = ".device_enqueue_symbol";
    static constexpr char KernargSegmentSize[]      = ".kernarg_segment_size";
    static constexpr char GroupSegmentFixedSize[]   = ".group_segment_fixed_size";
    static constexpr char PrivateSegmentFixedSize[] = ".private_segment_fixed_size";
    static constexpr char KernargSegmentAlign[]     = ".kernarg_segment_align";
    static constexpr char WavefrontSize[]           = ".wavefront_size";
    static constexpr char SgprCount[]               = ".sgpr_count";
    static constexpr char VgprCount[]               = ".vgpr_count";
    static constexpr char MaxFlatWorkgroupSize[]    = ".max_flat_workgroup_size";
    static constexpr char SgprSpillCount[]          = ".sgpr_spill_count";
    static constexpr char VgprSpillCount[]          = ".vgpr_spill_count";
    static constexpr char Kind[]                    = ".kind";
    static constexpr char UsesDynamicStack[]        = ".uses_dynamic_stack";
    static constexpr char WorkgroupProcessorMode[]  = ".workgroup_processor_mode";
    static constexpr char UniformWorkgroupSize[]    = ".uniform_work_group_size";
};

// =====================================================================================================================
// Translates the reader's next item, which must be a string, into a ValueKind.
static Result UnpackNextValueKind(
    MsgPackReader* pReader,
    ValueKind*     pEnum)
{
    Result result = pReader->Next(CWP_ITEM_STR);

    if (result == Result::Success)
    {
        const cwpack_blob& str = pReader->Get().as.str;

        switch(HashString(static_cast<const char*>(str.start), str.length))
        {
        case CompileTimeHashString("by_value"):
            *pEnum = ValueKind::ByValue;
            break;
        case CompileTimeHashString("global_buffer"):
            *pEnum = ValueKind::GlobalBuffer;
            break;
        case CompileTimeHashString("dynamic_shared_pointer"):
            *pEnum = ValueKind::DynamicSharedPointer;
            break;
        case CompileTimeHashString("sampler"):
            *pEnum = ValueKind::Sampler;
            break;
        case CompileTimeHashString("image"):
            *pEnum = ValueKind::Image;
            break;
        case CompileTimeHashString("pipe"):
            *pEnum = ValueKind::Pipe;
            break;
        case CompileTimeHashString("queue"):
            *pEnum = ValueKind::Queue;
            break;
        case CompileTimeHashString("hidden_global_offset_x"):
            *pEnum = ValueKind::HiddenGlobalOffsetX;
            break;
        case CompileTimeHashString("hidden_global_offset_y"):
            *pEnum = ValueKind::HiddenGlobalOffsetY;
            break;
        case CompileTimeHashString("hidden_global_offset_z"):
            *pEnum = ValueKind::HiddenGlobalOffsetZ;
            break;
        case CompileTimeHashString("hidden_none"):
            *pEnum = ValueKind::HiddenNone;
            break;
        case CompileTimeHashString("hidden_printf_buffer"):
            *pEnum = ValueKind::HiddenPrintfBuffer;
            break;
        case CompileTimeHashString("hidden_hostcall_buffer"):
            *pEnum = ValueKind::HiddenHostcallBuffer;
            break;
        case CompileTimeHashString("hidden_default_queue"):
            *pEnum = ValueKind::HiddenDefaultQueue;
            break;
        case CompileTimeHashString("hidden_completion_action"):
            *pEnum = ValueKind::HiddenCompletionAction;
            break;
        case CompileTimeHashString("hidden_multigrid_sync_arg"):
            *pEnum = ValueKind::HiddenMultigridSyncArg;
            break;
        case CompileTimeHashString("hidden_block_count_x"):
            *pEnum = ValueKind::HiddenBlockCountX;
            break;
        case CompileTimeHashString("hidden_block_count_y"):
            *pEnum = ValueKind::HiddenBlockCountY;
            break;
        case CompileTimeHashString("hidden_block_count_z"):
            *pEnum = ValueKind::HiddenBlockCountZ;
            break;
        case CompileTimeHashString("hidden_group_size_x"):
            *pEnum = ValueKind::HiddenGroupSizeX;
            break;
        case CompileTimeHashString("hidden_group_size_y"):
            *pEnum = ValueKind::HiddenGroupSizeY;
            break;
        case CompileTimeHashString("hidden_group_size_z"):
            *pEnum = ValueKind::HiddenGroupSizeZ;
            break;
        case CompileTimeHashString("hidden_remainder_x"):
            *pEnum = ValueKind::HiddenRemainderX;
            break;
        case CompileTimeHashString("hidden_remainder_y"):
            *pEnum = ValueKind::HiddenRemainderY;
            break;
        case CompileTimeHashString("hidden_remainder_z"):
            *pEnum = ValueKind::HiddenRemainderZ;
            break;
        case CompileTimeHashString("hidden_grid_dims"):
            *pEnum = ValueKind::HiddenGridDims;
            break;
        case CompileTimeHashString("hidden_heap_v1"):
            *pEnum = ValueKind::HiddenHeapV1;
            break;
        case CompileTimeHashString("hidden_dynamic_lds_size"):
            *pEnum = ValueKind::HiddenDynamicLdsSize;
            break;
        case CompileTimeHashString("hidden_queue_ptr"):
            *pEnum = ValueKind::HiddenQueuePtr;
            break;
        default:
            // This probably means we have a bug in this code rather than a bad metadata section.
            PAL_ASSERT_ALWAYS();
            result = Result::ErrorInvalidValue;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
// Translates the reader's next item, which must be a string, into an AddressSpace.
static Result UnpackNextAddressSpace(
    MsgPackReader* pReader,
    AddressSpace*  pEnum)
{
    Result result = pReader->Next(CWP_ITEM_STR);

    if (result == Result::Success)
    {
        const cwpack_blob& str = pReader->Get().as.str;

        switch(HashString(static_cast<const char*>(str.start), str.length))
        {
        case CompileTimeHashString("private"):
            *pEnum = AddressSpace::Private;
            break;
        case CompileTimeHashString("global"):
            *pEnum = AddressSpace::Global;
            break;
        case CompileTimeHashString("constant"):
            *pEnum = AddressSpace::Constant;
            break;
        case CompileTimeHashString("local"):
            *pEnum = AddressSpace::Local;
            break;
        case CompileTimeHashString("generic"):
            *pEnum = AddressSpace::Generic;
            break;
        case CompileTimeHashString("region"):
            *pEnum = AddressSpace::Region;
            break;
        default:
            // This probably means we have a bug in this code rather than a bad metadata section.
            PAL_ASSERT_ALWAYS();
            result = Result::ErrorInvalidValue;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
// Translates the reader's next item, which must be a string, into an Access.
static Result UnpackNextAccess(
    MsgPackReader* pReader,
    Access*        pEnum)
{
    Result result = pReader->Next(CWP_ITEM_STR);

    if (result == Result::Success)
    {
        const cwpack_blob& str = pReader->Get().as.str;

        switch(HashString(static_cast<const char*>(str.start), str.length))
        {
        case CompileTimeHashString("read_only"):
            *pEnum = Access::ReadOnly;
            break;
        case CompileTimeHashString("write_only"):
            *pEnum = Access::WriteOnly;
            break;
        case CompileTimeHashString("read_write"):
            *pEnum = Access::ReadWrite;
            break;
        default:
            // This probably means we have a bug in this code rather than a bad metadata section.
            PAL_ASSERT_ALWAYS();
            result = Result::ErrorInvalidValue;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
// Translates the reader's next item, which must be a string, into a Kind.
static Result UnpackNextKind(
    MsgPackReader* pReader,
    Kind*          pEnum)
{
    Result result = pReader->Next(CWP_ITEM_STR);

    if (result == Result::Success)
    {
        const cwpack_blob& str = pReader->Get().as.str;

        switch(HashString(static_cast<const char*>(str.start), str.length))
        {
        case CompileTimeHashString("normal"):
            *pEnum = Kind::Normal;
            break;
        case CompileTimeHashString("init"):
            *pEnum = Kind::Init;
            break;
        case CompileTimeHashString("fini"):
            *pEnum = Kind::Fini;
            break;
        default:
            // This probably means we have a bug in this code rather than a bad metadata section.
            PAL_ASSERT_ALWAYS();
            result = Result::ErrorInvalidValue;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
// Translates the reader's next item, which must be a string, into a PAL_MALLOCed buffer.
static Result UnpackNextString(
    MsgPackReader*     pReader,
    IndirectAllocator* pAllocator,
    const char**       ppString)
{
    Result result = pReader->Next(CWP_ITEM_STR);

    if (result == Result::Success)
    {
        // Note that cwpack string lengths don't include a null terminator.
        const uint32 strSize = pReader->Get().as.str.length + 1;
        char*const   pPalStr = static_cast<char*>(PAL_MALLOC(strSize, pAllocator, AllocInternal));

        if (pPalStr != nullptr)
        {
            result = pReader->Unpack(pPalStr, strSize);

            if (result == Result::Success)
            {
                *ppString = pPalStr;
            }
            else
            {
                PAL_FREE(pPalStr, pAllocator);
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Check the current kernel matching the kernelName.
// Pass reader by value to avoid changing the original reader.
static Result IsMatchingKernelName(
    MsgPackReader    reader,
    StringView<char> kernelName,
    bool*            pNameMatches)
{
    PAL_ASSERT(kernelName.IsEmpty() == false);

    bool found     = false;
    StringView<char> name;

    Result result = reader.Next(CWP_ITEM_MAP);

    for (uint32 i = reader.Get().as.map.size; ((result == Result::Success) && (found == false) && (i > 0)); --i)
    {
        StringView<char> keyName;
        result = reader.UnpackNext(&keyName);

        if (result == Result::Success)
        {
            switch (HashString(keyName))
            {
            case CompileTimeHashString(KernelMetadataKey::Name):
                found = true;
                result = reader.UnpackNext(&name);
                break;

            default:
                result = reader.Skip(1);
                break;
            }
        }
    }

    if (result == Result::Success)
    {
        *pNameMatches = (name == kernelName);
    }

    return result;
}

// =====================================================================================================================
CodeObjectMetadata::~CodeObjectMetadata()
{
    for (uint32 i = 0; i < m_numArgs; ++i)
    {
        PAL_SAFE_FREE(m_pArgs[i].pName,     &m_allocator);
        PAL_SAFE_FREE(m_pArgs[i].pTypeName, &m_allocator);
    }

    PAL_DELETE_ARRAY(m_pArgs, &m_allocator);

    PAL_SAFE_FREE(m_pName,                &m_allocator);
    PAL_SAFE_FREE(m_pSymbol,              &m_allocator);
    PAL_SAFE_FREE(m_pLanguage,            &m_allocator);
    PAL_SAFE_FREE(m_pVecTypeHint,         &m_allocator);
    PAL_SAFE_FREE(m_pDeviceEnqueueSymbol, &m_allocator);
}

// =====================================================================================================================
Result CodeObjectMetadata::DeserializeKernelArgs(
    MsgPackReader* pReader)
{
    Result result = pReader->Next(CWP_ITEM_ARRAY);

    if (result == Result::Success)
    {
        m_numArgs = pReader->Get().as.array.size;
        m_pArgs   = PAL_NEW_ARRAY(KernelArgument, m_numArgs, &m_allocator, AllocInternal);

        if (m_pArgs == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            // This is just an array of dumb structs so we should zero it out manually.
            memset(m_pArgs, 0, sizeof(KernelArgument) * m_numArgs);
        }
    }

    for (uint32 i = 0; ((result == Result::Success) && (i < m_numArgs)); ++i)
    {
        KernelArgument*const pArg = m_pArgs + i;

        // For sanity's sake, track that we see each metadata key only once.
        union
        {
            struct
            {
                uint32 name         : 1;
                uint32 typeName     : 1;
                uint32 size         : 1;
                uint32 offset       : 1;
                uint32 valueKind    : 1;
                uint32 pointeeAlign : 1;
                uint32 addressSpace : 1;
                uint32 access       : 1;
                uint32 actualAccess : 1;
                uint32 reserved     : 24;
            };
            uint32 u32All;
        } hasEntry = {};

        result = pReader->Next(CWP_ITEM_MAP);

        for (uint32 j = pReader->Get().as.map.size; ((result == Result::Success) && (j > 0)); --j)
        {
            result = pReader->Next();

            if (result == Result::Success)
            {
                bool flag;
                const cwpack_blob& str = pReader->Get().as.str;

                // These cases follow the same order as the spec.
                switch(HashString(static_cast<const char*>(str.start), str.length))
                {
                case CompileTimeHashString(KernArgsMetadataKey::Name):
                    PAL_ASSERT(hasEntry.name == 0);
                    hasEntry.name = 1;
                    result = UnpackNextString(pReader, &m_allocator, &pArg->pName);
                    break;
                case CompileTimeHashString(KernArgsMetadataKey::TypeName):
                    PAL_ASSERT(hasEntry.typeName == 0);
                    hasEntry.typeName = 1;
                    result = UnpackNextString(pReader, &m_allocator, &pArg->pTypeName);
                    break;
                case CompileTimeHashString(KernArgsMetadataKey::Size):
                    PAL_ASSERT(hasEntry.size == 0);
                    hasEntry.size = 1;
                    result = pReader->UnpackNext(&pArg->size);
                    break;
                case CompileTimeHashString(KernArgsMetadataKey::Offset):
                    PAL_ASSERT(hasEntry.offset == 0);
                    hasEntry.offset = 1;
                    result = pReader->UnpackNext(&pArg->offset);
                    break;
                case CompileTimeHashString(KernArgsMetadataKey::ValueKind):
                    PAL_ASSERT(hasEntry.valueKind == 0);
                    hasEntry.valueKind = 1;
                    result = UnpackNextValueKind(pReader, &pArg->valueKind);
                    break;
                case CompileTimeHashString(KernArgsMetadataKey::PointeeAlign):
                    PAL_ASSERT(hasEntry.pointeeAlign == 0);
                    hasEntry.pointeeAlign = 1;
                    result = pReader->UnpackNext(&pArg->pointeeAlign);
                    break;
                case CompileTimeHashString(KernArgsMetadataKey::AddressSpace):
                    PAL_ASSERT(hasEntry.addressSpace == 0);
                    hasEntry.addressSpace = 1;
                    result = UnpackNextAddressSpace(pReader, &pArg->addressSpace);
                    break;
                case CompileTimeHashString(KernArgsMetadataKey::Access):
                    PAL_ASSERT(hasEntry.access == 0);
                    hasEntry.access = 1;
                    result = UnpackNextAccess(pReader, &pArg->access);
                    break;
                case CompileTimeHashString(KernArgsMetadataKey::ActualAccess):
                    PAL_ASSERT(hasEntry.actualAccess == 0);
                    hasEntry.actualAccess = 1;
                    result = UnpackNextAccess(pReader, &pArg->actualAccess);
                    break;
                case CompileTimeHashString(KernArgsMetadataKey::IsConst):
                    PAL_ASSERT(pArg->flags.isConst == 0);
                    result = pReader->UnpackNext(&flag);
                    pArg->flags.isConst = flag;
                    break;
                case CompileTimeHashString(KernArgsMetadataKey::IsPipe):
                    PAL_ASSERT(pArg->flags.isPipe == 0);
                    result = pReader->UnpackNext(&flag);
                    pArg->flags.isPipe = flag;
                    break;
                case CompileTimeHashString(KernArgsMetadataKey::IsRestrict):
                    PAL_ASSERT(pArg->flags.isRestrict == 0);
                    result = pReader->UnpackNext(&flag);
                    pArg->flags.isRestrict = flag;
                    break;
                case CompileTimeHashString(KernArgsMetadataKey::IsVolatile):
                    PAL_ASSERT(pArg->flags.isVolatile == 0);
                    result = pReader->UnpackNext(&flag);
                    pArg->flags.isVolatile = flag;
                    break;
                default:
                    // Note that we don't extract some valid keys because we don't use them.
                    result = pReader->Next();
                    break;
                }
            }
        }

        // These values are required by the spec. We can reject the ELF in the parser if they're missing.
        if ((result == Result::Success) &&
            ((hasEntry.size == 0) || (hasEntry.offset == 0) || (hasEntry.valueKind == 0)))
        {
            result = Result::ErrorInvalidPipelineElf;
        }
    }

    return result;
}

// =====================================================================================================================
// Deserialize the kernels array.
Result CodeObjectMetadata::DeserializeKernel(
    MsgPackReader*   pReader)
{
    // Each array element is a map.
    Result result = pReader->Next(CWP_ITEM_MAP);

    // For sanity's sake, track that we see each metadata key only once.
    union
    {
        struct
        {
            uint32 name                    : 1;
            uint32 symbol                  : 1;
            uint32 language                : 1;
            uint32 languageVersion         : 1;
            uint32 args                    : 1;
            uint32 reqdWorkgroupSize       : 1;
            uint32 workgroupSizeHint       : 1;
            uint32 vecTypeHint             : 1;
            uint32 deviceEnqueueSymbol     : 1;
            uint32 kernargSegmentSize      : 1;
            uint32 groupSegmentFixedSize   : 1;
            uint32 privateSegmentFixedSize : 1;
            uint32 kernargSegmentAlign     : 1;
            uint32 wavefrontSize           : 1;
            uint32 sgprCount               : 1;
            uint32 vgprCount               : 1;
            uint32 maxFlatWorkgroupSize    : 1;
            uint32 sgprSpillCount          : 1;
            uint32 vgprSpillCount          : 1;
            uint32 kind                    : 1;
            uint32 uniformWorkgroupSize    : 1;
            uint32 usesDynamicStack        : 1;
            uint32 workgroupProcessorMode  : 1;
            uint32 reserved                : 9;
        };
        uint32 u32All;
    } hasEntry = {};

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        result = pReader->Next(CWP_ITEM_STR);

        if (result == Result::Success)
        {
            const cwpack_blob& str = pReader->Get().as.str;

            // These cases follow the same order as the spec.
            switch (HashString(static_cast<const char*>(str.start), str.length))
            {
            case CompileTimeHashString(KernelMetadataKey::Name):
                PAL_ASSERT(hasEntry.name == 0);
                hasEntry.name = 1;
                result = UnpackNextString(pReader, &m_allocator, &m_pName);
                break;
            case CompileTimeHashString(KernelMetadataKey::Symbol):
                PAL_ASSERT(hasEntry.symbol == 0);
                hasEntry.symbol = 1;
                result = UnpackNextString(pReader, &m_allocator, &m_pSymbol);
                break;
            case CompileTimeHashString(KernelMetadataKey::Language):
                PAL_ASSERT(hasEntry.language == 0);
                hasEntry.language = 1;
                result = UnpackNextString(pReader, &m_allocator, &m_pLanguage);
                break;
            case CompileTimeHashString(KernelMetadataKey::LanguageVersion):
                PAL_ASSERT(hasEntry.languageVersion == 0);
                hasEntry.languageVersion = 1;
                result = pReader->UnpackNext(&m_languageVersion);
                break;
            case CompileTimeHashString(KernelMetadataKey::Args):
                PAL_ASSERT(hasEntry.args == 0);
                hasEntry.args = 1;
                result = DeserializeKernelArgs(pReader);
                break;
            case CompileTimeHashString(KernelMetadataKey::ReqdWorkgroupSize):
                PAL_ASSERT(hasEntry.reqdWorkgroupSize == 0);
                hasEntry.reqdWorkgroupSize = 1;
                result = pReader->UnpackNext(&m_reqdWorkgroupSize);
                break;
            case CompileTimeHashString(KernelMetadataKey::WorkgroupSizeHint):
                PAL_ASSERT(hasEntry.workgroupSizeHint == 0);
                hasEntry.workgroupSizeHint = 1;
                result = pReader->UnpackNext(&m_workgroupSizeHint);
                break;
            case CompileTimeHashString(KernelMetadataKey::VecTypeHint):
                PAL_ASSERT(hasEntry.vecTypeHint == 0);
                hasEntry.vecTypeHint = 1;
                result = UnpackNextString(pReader, &m_allocator, &m_pVecTypeHint);
                break;
            case CompileTimeHashString(KernelMetadataKey::DeviceEnqueueSymbol):
                PAL_ASSERT(hasEntry.deviceEnqueueSymbol == 0);
                hasEntry.deviceEnqueueSymbol = 1;
                result = UnpackNextString(pReader, &m_allocator, &m_pDeviceEnqueueSymbol);
                break;
            case CompileTimeHashString(KernelMetadataKey::KernargSegmentSize):
                PAL_ASSERT(hasEntry.kernargSegmentSize == 0);
                hasEntry.kernargSegmentSize = 1;
                result = pReader->UnpackNext(&m_kernargSegmentSize);
                break;
            case CompileTimeHashString(KernelMetadataKey::GroupSegmentFixedSize):
                PAL_ASSERT(hasEntry.groupSegmentFixedSize == 0);
                hasEntry.groupSegmentFixedSize = 1;
                result = pReader->UnpackNext(&m_groupSegmentFixedSize);
                break;
            case CompileTimeHashString(KernelMetadataKey::PrivateSegmentFixedSize):
                PAL_ASSERT(hasEntry.privateSegmentFixedSize == 0);
                hasEntry.privateSegmentFixedSize = 1;
                result = pReader->UnpackNext(&m_privateSegmentFixedSize);
                break;
            case CompileTimeHashString(KernelMetadataKey::KernargSegmentAlign):
                PAL_ASSERT(hasEntry.kernargSegmentAlign == 0);
                hasEntry.kernargSegmentAlign = 1;
                result = pReader->UnpackNext(&m_kernargSegmentAlign);
                break;
            case CompileTimeHashString(KernelMetadataKey::WavefrontSize):
                PAL_ASSERT(hasEntry.wavefrontSize == 0);
                hasEntry.wavefrontSize = 1;
                result = pReader->UnpackNext(&m_wavefrontSize);
                break;
            case CompileTimeHashString(KernelMetadataKey::SgprCount):
                PAL_ASSERT(hasEntry.sgprCount == 0);
                hasEntry.sgprCount = 1;
                result = pReader->UnpackNext(&m_sgprCount);
                break;
            case CompileTimeHashString(KernelMetadataKey::VgprCount):
                PAL_ASSERT(hasEntry.vgprCount == 0);
                hasEntry.vgprCount = 1;
                result = pReader->UnpackNext(&m_vgprCount);
                break;
            case CompileTimeHashString(KernelMetadataKey::MaxFlatWorkgroupSize):
                PAL_ASSERT(hasEntry.maxFlatWorkgroupSize == 0);
                hasEntry.maxFlatWorkgroupSize = 1;
                result = pReader->UnpackNext(&m_maxFlatWorkgroupSize);
                break;
            case CompileTimeHashString(KernelMetadataKey::SgprSpillCount):
                PAL_ASSERT(hasEntry.sgprSpillCount == 0);
                hasEntry.sgprSpillCount = 1;
                result = pReader->UnpackNext(&m_sgprSpillCount);
                break;
            case CompileTimeHashString(KernelMetadataKey::VgprSpillCount):
                PAL_ASSERT(hasEntry.vgprSpillCount == 0);
                hasEntry.vgprSpillCount = 1;
                result = pReader->UnpackNext(&m_vgprSpillCount);
                break;
            case CompileTimeHashString(KernelMetadataKey::Kind):
                PAL_ASSERT(hasEntry.kind == 0);
                hasEntry.kind = 1;
                result = UnpackNextKind(pReader, &m_kind);
                break;
            case CompileTimeHashString(KernelMetadataKey::UniformWorkgroupSize):
                PAL_ASSERT(hasEntry.workgroupProcessorMode == 0);
                hasEntry.uniformWorkgroupSize = 1;
                result = pReader->UnpackNext(&m_uniformWorkgroupSize);
                break;
            case CompileTimeHashString(KernelMetadataKey::UsesDynamicStack):
                PAL_ASSERT(hasEntry.usesDynamicStack == 0);
                hasEntry.usesDynamicStack = 1;
                result = pReader->UnpackNext(&m_usesDynamicStack);
                break;
            case CompileTimeHashString(KernelMetadataKey::WorkgroupProcessorMode):
                PAL_ASSERT(hasEntry.workgroupProcessorMode == 0);
                hasEntry.workgroupProcessorMode = 1;
                result = pReader->UnpackNext(&m_workgroupProcessorMode);
                break;
            default:
                // Note that we don't extract some valid keys because we don't use them.
                result = pReader->Skip(1);
                break;
            }
        }
    }

    // These values are required by the spec. We can reject the ELF in the parser if they're missing.
    if ((result == Result::Success) &&
        ((hasEntry.name                    == 0) ||
         (hasEntry.symbol                  == 0) ||
         (hasEntry.kernargSegmentSize      == 0) ||
         (hasEntry.groupSegmentFixedSize   == 0) ||
         (hasEntry.privateSegmentFixedSize == 0) ||
         (hasEntry.kernargSegmentAlign     == 0) ||
         (hasEntry.wavefrontSize           == 0) ||
         (hasEntry.sgprCount               == 0) ||
         (hasEntry.vgprCount               == 0) ||
         (hasEntry.maxFlatWorkgroupSize    == 0)))
    {
        result = Result::ErrorInvalidPipelineElf;
    }
    else if (((m_reqdWorkgroupSize[0] == 0) != (m_reqdWorkgroupSize[1] == 0)) ||
             ((m_reqdWorkgroupSize[0] == 0) != (m_reqdWorkgroupSize[2] == 0)))
    {
        // The three required workgroup sizes must all be zero or all non-zero.
        result = Result::ErrorInvalidPipelineElf;
    }

    return result;
}

// =====================================================================================================================
Result CodeObjectMetadata::SetVersion(
    uint32 metadataMajorVer,
    uint32 metadataMinorVer)
{
    m_codeVersionMajor = metadataMajorVer;
    m_codeVersionMinor = metadataMinorVer;

    // The current metadata version is 1.0. We assume minor changes are backwards compatible but major changes are not.
    constexpr uint32 HsaMetadataMajorVersion = 1u;
    if(m_codeVersionMinor < 2)
    {
        // metadata v5 changes some semantics, before it, always uniform.
        m_uniformWorkgroupSize = 1;
    }
    return (metadataMajorVer == HsaMetadataMajorVersion) ? Result::Success
                                                         : Result::ErrorUnsupportedPipelineElfAbiVersion;
}

// =====================================================================================================================
Result CodeObjectMetadata::DeserializeNote(
    MsgPackReader*   pReader,
    const void*      pRawMetadata,
    uint32           metadataSize,
    StringView<char> kernelName)
{
    // Reset the msgpack reader, it was previously used to grab the version.
    Result result = pReader->InitFromBuffer(pRawMetadata, metadataSize);

    // The first item must be a map.
    if ((result == Result::Success) && (pReader->Type() != CWP_ITEM_MAP))
    {
        result = Result::ErrorInvalidValue;
    }

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        result = pReader->Next(CWP_ITEM_STR);

        if (result == Result::Success)
        {
            const cwpack_blob& str = pReader->Get().as.str;

            switch (HashString(static_cast<const char*>(str.start), str.length))
            {
            case CompileTimeHashString(PipelineMetadataKey::Kernels):
                // Handle the kernels array.
                result = pReader->Next(CWP_ITEM_ARRAY);

                if (result == Result::Success)
                {
                    if (pReader->Get().as.array.size == 1)
                    {
                        // There is only one kernel in the array, skip check kernelName.
                        result = DeserializeKernel(pReader);
                    }
                    else if (kernelName.IsEmpty())
                    {
                        // There are multiple kernels in the array, must set kernelName.
                        result = Result::ErrorInvalidValue;
                    }
                    else
                    {
                        bool found = false;
                        for (uint32 k = pReader->Get().as.array.size; ((result == Result::Success) && (k > 0)); --k)
                        {
                            bool nameMatches = false;
                            result = (found == false) ? IsMatchingKernelName(*pReader, kernelName, &nameMatches) :
                                                        Result::Success;

                            if ((found == false) && (result == Result::Success) && (nameMatches))
                            {
                                found = true; // Found the kernel, skip following ones.
                                result = DeserializeKernel(pReader);
                            }
                            else
                            {
                                result = pReader->Skip(1);
                            }
                        }
                    }
                }
                break;
            default:
                // Note that we don't extract some valid keys because we don't use them.
                result = pReader->Skip(1);
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
uint32 CodeObjectMetadata::PrivateSegmentFixedSize() const
{
    // Dynamic Stack can happen if recursive calls, calls to indirect functions, or the HSAIL alloca instruction
    // are present in the kernel.
    // Dynamic stack size is hard to calcuate by both runtime and application. The reason it exists could be recursive
    // function call, indirect function call, etc. They are unknown at compile time, and almost impossible to be known
    // at kernel launch time as well.The actual usage depends on the execution path of the kernel. For example, how deep
    // can a recursive function call be ? What can a virtual function call be ? One callee might use 16 bytes stack but
    // the other could use 16 MB. The only thing we can do is to set a limit, and the kernel will crash when the limit
    // is not sufficient.
    // Here just use 16KB as default as OpenCL runtime.

    constexpr uint32 DefaultStackSize = 16 * 1024; // 16 KB as OpenCL runtime default
    return UsesDynamicStack() ? Max(DefaultStackSize, m_privateSegmentFixedSize) : m_privateSegmentFixedSize;
}

} // HsaAbi
} // Util
