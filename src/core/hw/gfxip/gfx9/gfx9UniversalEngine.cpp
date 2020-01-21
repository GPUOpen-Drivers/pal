/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9UniversalEngine.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRingSet.h"

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
UniversalEngine::UniversalEngine(
    Device*    pDevice,
    EngineType type,
    uint32     index)
    :
    Engine(*pDevice->Parent(), type, index),
    m_pDevice(pDevice),
    m_ringSet(pDevice),
    m_currentUpdateCounter(0)
{
}

// =====================================================================================================================
Result UniversalEngine::Init()
{
    Result result = Engine::Init();

    if (result == Result::Success)
    {
        result = m_ringSet.Init();
    }

    return result;
}

// =====================================================================================================================
Result UniversalEngine::UpdateRingSet(
    uint32* pCounterVal,  // [in, out] As input is the currently known counter value of the QueueContext.
                          //           On output, this is the current counter value.
    bool*   pHasChanged)  // [out]     Whether or not the ring set has updated. If true the ring set must rewrite its
                          //           registers.
{
    PAL_ALERT((pCounterVal == nullptr) || (pHasChanged == nullptr));

    Result result = Result::Success;

    // Check if the queue context associated with this Queue is dirty, and obtain the ring item-sizes to validate
    // against.
    const uint32 currentCounter = m_pDevice->QueueContextUpdateCounter();

    if (currentCounter > m_currentUpdateCounter)
    {
        m_currentUpdateCounter = currentCounter;

        ShaderRingItemSizes ringSizes = {};
        m_pDevice->GetLargestRingSizes(&ringSizes);

        SamplePatternPalette samplePatternPalette;
        m_pDevice->GetSamplePatternPalette(&samplePatternPalette);

        // The ring-set may be dirty. First, we need to idle all queues so that we can reallocate the rings and update
        // the ring-set's SRD table.
        // This wait-for-idle is expensive, but it is expected that after a few frames, the application will reach a
        // steady-state and no longer need to do any validation at submit-time.
        //
        // NOTE: If a batched command generates a submit which triggers ring validation we are in deep trouble because
        // some of the commands further down in the batched queue might assume that the preamble stream hasn't been
        // rebuilt. To prevent this, this preprocessing is done before the submission has a chance to be batched.
        result = WaitIdleAllQueues();

        // The queues are idle, so it is safe to validate the rest of the RingSet.
        if (result == Result::Success)
        {
            result = m_ringSet.Validate(ringSizes, samplePatternPalette);
        }
    }

    (*pHasChanged) = (m_currentUpdateCounter > (*pCounterVal));
    (*pCounterVal) = m_currentUpdateCounter;

    return result;
}

} // Gfx9
} // Pal
