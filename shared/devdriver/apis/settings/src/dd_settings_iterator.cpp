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

#include <dd_settings_iterator.h>
#include <dd_assert.h>

#include <cstring>

namespace DevDriver
{

SettingsIterator::SettingsIterator(const uint8_t* pBuf, size_t size)
    : m_pBuf {pBuf}
    , m_bufSize {size}
    , m_allComponentsHeader {}
    , m_error {DD_RESULT_SUCCESS}
{
    if (m_bufSize >= sizeof(m_allComponentsHeader))
    {
        memcpy(&m_allComponentsHeader, m_pBuf, sizeof(m_allComponentsHeader));
    }
    else
    {
        m_error = DD_RESULT_COMMON_BUFFER_TOO_SMALL;
    }

    if (m_allComponentsHeader.version > 1)
    {
        m_error = DD_RESULT_COMMON_VERSION_MISMATCH;
    }

    if (m_allComponentsHeader.numComponents == 0)
    {
        m_error = DD_RESULT_COMMON_DOES_NOT_EXIST;
    }
}

bool SettingsIterator::NextComponent(Component* pComponent)
{
    bool foundNextComponent = false;

    if (m_error == DD_RESULT_SUCCESS)
    {
        size_t nextComponentOffset = 0;
        if (pComponent->offset == 0)
        {
            // It's the first time this function is called.
            nextComponentOffset = sizeof(m_allComponentsHeader);
        }
        else
        {
            DD_ASSERT(pComponent->offset >= sizeof(m_allComponentsHeader));
            size_t currComponentOffset = pComponent->offset;

            const DDSettingsComponentHeader* pCurrCompHeader = nullptr;
            DD_ASSERT(currComponentOffset <= (m_bufSize - sizeof(*pCurrCompHeader)));
            pCurrCompHeader = reinterpret_cast<const DDSettingsComponentHeader*>(m_pBuf + currComponentOffset);

            nextComponentOffset = currComponentOffset + pCurrCompHeader->size;
            DD_ASSERT(nextComponentOffset > currComponentOffset);
        }

        if (nextComponentOffset < m_bufSize)
        {
            auto pNextCompHeader = reinterpret_cast<const DDSettingsComponentHeader*>(m_pBuf + nextComponentOffset);
            pComponent->pName     = pNextCompHeader->name;
            pComponent->blobHash  = pNextCompHeader->blobHash;
            pComponent->numValues = pNextCompHeader->numValues;
            pComponent->offset    = nextComponentOffset;

            foundNextComponent = true;
        }
        else
        {
            // The current component is the last one. There is no next component.
        }
    }

    return foundNextComponent;
}

bool SettingsIterator::NextValue(const Component& component, Value* pValue)
{
    bool foundNextValue = false;

    if (m_error == DD_RESULT_SUCCESS)
    {
        DD_ASSERT(component.offset >= sizeof(m_allComponentsHeader));

        const uint8_t* pCurrCompAddr = m_pBuf + component.offset;
        auto pCurrCompHeader = reinterpret_cast<const DDSettingsComponentHeader*>(pCurrCompAddr);

        size_t nextValueOffset = 0;
        if (pValue->offset == 0)
        {
            nextValueOffset = sizeof(*pCurrCompHeader);
        }
        else
        {
            size_t currValueOffset = pValue->offset;
            DD_ASSERT(currValueOffset >= sizeof(*pCurrCompHeader));
            DD_ASSERT(currValueOffset < pCurrCompHeader->size);

            const DDSettingsValueHeader* pCurrValueHeader = nullptr;
            pCurrValueHeader = reinterpret_cast<const DDSettingsValueHeader*>(pCurrCompAddr + pValue->offset);

            nextValueOffset = currValueOffset + sizeof(*pCurrValueHeader) + pCurrValueHeader->valueSize;
            DD_ASSERT(nextValueOffset > currValueOffset);
        }

        if (nextValueOffset < pCurrCompHeader->size)
        {
            auto pNextValueHeader = reinterpret_cast<const DDSettingsValueHeader*>(pCurrCompAddr + nextValueOffset);

            pValue->valueRef.hash   = pNextValueHeader->hash;
            pValue->valueRef.type   = pNextValueHeader->type;
            pValue->valueRef.size   = pNextValueHeader->valueSize;
            pValue->valueRef.pValue = (void*)(pCurrCompAddr + nextValueOffset + sizeof(*pNextValueHeader));
            pValue->offset          = nextValueOffset;

            foundNextValue = true;
        }
        else
        {
            // The current value is the last one. There is no next value.
        }
    }

    return foundNextValue;
}

bool SettingsIterator::NextUnsupportedExperiment(const Component& component, UnsupportedExperiment* pExp)
{
    bool foundNextValue = false;

    if (m_error == DD_RESULT_SUCCESS)
    {
        DD_ASSERT(component.offset >= sizeof(m_allComponentsHeader));

        const uint8_t* pCurrCompAddr   = m_pBuf + component.offset;
        auto           pCurrCompHeader = reinterpret_cast<const DDSettingsComponentHeader*>(pCurrCompAddr);

        size_t nextValueOffset = 0;
        if (pExp->offset == 0)
        {
            nextValueOffset = sizeof(*pCurrCompHeader);
        }
        else
        {
            size_t currValueOffset = pExp->offset;
            DD_ASSERT(currValueOffset >= sizeof(*pCurrCompHeader));
            DD_ASSERT(currValueOffset < pCurrCompHeader->size);

            const DD_SETTINGS_NAME_HASH* pCurrValueHeader = nullptr;
            pCurrValueHeader = reinterpret_cast<const DD_SETTINGS_NAME_HASH*>(pCurrCompAddr + pExp->offset);

            nextValueOffset = currValueOffset + sizeof(*pCurrValueHeader);
            DD_ASSERT(nextValueOffset > currValueOffset);
        }

        if (nextValueOffset < pCurrCompHeader->size)
        {
            auto pNextValueHeader = reinterpret_cast<const DD_SETTINGS_NAME_HASH*>(pCurrCompAddr + nextValueOffset);

            pExp->hash   = *pNextValueHeader;
            pExp->offset = nextValueOffset;

            foundNextValue = true;
        }
        else
        {
            // The current experiment is the last one. There is no next value.
        }
    }

    return foundNextValue;
}

} // namespace DevDriver
