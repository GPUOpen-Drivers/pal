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

#include <dd_settings_rpc_types.h>
#include <dd_settings_base.h>
#include <dd_mutex.h>

#include <g_SettingsRpcService2.h>

#include <util/hashMap.h>
#include <util/vector.h>
#include <ddPlatform.h>

namespace DevDriver
{

class SettingsRpcService: public SettingsRpc::ISettingsRpcService
{
private:
    AllocCb m_allocCb;

    HashMap<const char*, SettingsBase*> m_settingsComponents;
    Mutex                               m_settingsComponentsMutex;

    // User-overrides for all settings components.
    uint8_t* m_pAllUserOverridesData;
    HashMap<const char*, Vector<DDSettingsValueRef>> m_allUserOverrides;

public:
    SettingsRpcService();
    ~SettingsRpcService();

    // Register a settings component to the settings rpc service. Also apply user-overrides to the registered
    // component if available.
    void RegisterSettingsComponent(SettingsBase* pSettingsComponent);

    // Apply all available user-overrides to the settings component pointed to by `pSettingsComponent`.
    void ApplyComponentUserOverrides(SettingsBase* pSettingsComponent);

    // Apply a single user-override identified by `nameHash`.
    bool ApplyUserOverride(
        SettingsBase*         pSettingsComponent,
        DD_SETTINGS_NAME_HASH nameHash,
        void*                 pSetting,
        size_t                settingSize);

    // Get number of user-overrides across all settings components.
    size_t TotalUserOverrideCount() const;

    // Settings RPC implementations
    DD_RESULT SendAllUserOverrides(const void* pParamBuf, size_t paramBufSize) override;
    DD_RESULT QueryAllCurrentValues(const DDByteWriter& writer) override;
    DD_RESULT GetUnsupportedExperiments(const DDByteWriter& writer) override;
};

} // namespace DevDriver
