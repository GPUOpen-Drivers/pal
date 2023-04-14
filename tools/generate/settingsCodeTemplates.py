##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2023 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################

import os
import sys

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "offline"))
from genCopyright import Copyright

FileHeaderCopyright = f"/* {Copyright} */"

FileHeaderWarning = """
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!
//
// This code has been generated automatically. Do not hand-modify this code.
//
// When changes are needed, modify the tools generating this module in the tools\\generate directory OR
// settings_platform.json
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

"""

CopyrightAndWarning = FileHeaderCopyright + FileHeaderWarning

HeaderFileDoxComment = """
/**
***************************************************************************************************
* @file  %FileName%
* @brief auto-generated file.
*        Contains the definition for the PAL settings struct and enums for initialization.
***************************************************************************************************
*/
#pragma once
"""

NamespaceStart = """
namespace Pal
{
"""
HwlNamespaceStart = """namespace %Hwl%
{
"""
HwlNamespaceEnd   = """
} // %Hwl%"""
NamespaceEnd   = """
} // Pal"""

HeaderIncludes = """
#include \"pal.h\"
#include \"palFile.h\"
#include \"palSettingsLoader.h\"
"""

CppIncludes = """#include \"core/device.h\"
#include \"core/settingsLoader.h\"
"""

IncludeDir = ""
HwlIncludeDir = "core/hw/gfxip/%Hwl%/"

PrefixName = ""

DevDriverIncludes = """
#include \"devDriverServer.h\"
#include \"protocols/ddSettingsService.h\"
#include \"settingsService.h\"

using namespace DevDriver::SettingsURIService;
using namespace SettingsRpcService;

"""

Enum = """
enum %EnumName% : %EnumDataType%
{
%EnumData%
};
"""

StructDef = """
/// Pal auto-generated settings struct
struct %SettingStructName% : public Pal::DriverSettings
{
%SettingDefs%};
"""

SettingDef = "    %SettingType%    %SettingVarName%%ArrayLength%;\n"
SettingStructName = "%UpperCamelComponentName%Settings"
SettingStructDef = """    struct {
%StructSettingFields%    } %StructSettingName%;
"""

SettingStr = "constexpr const char* %SettingStrName% = %SettingString%;\n"

SetupDefaultsFunc = """
// =====================================================================================================================
// Initializes the settings structure to default values.
void %ClassName%::SetupDefaults()
{
    // set setting variables to their default values...
%SetDefaultsCode%
}
"""

IfMinMax = "#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= %MinVersion% && PAL_CLIENT_INTERFACE_MAJOR_VERSION <= %MaxVersion%\n"
IfMin    = "#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= %MinVersion%\n"
IfMax    = "#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= %MaxVersion%\n"
EndIf    = "#endif\n"

SetDefault = "    m_settings.%SettingVarName% = %SettingDefault%;\n"
SetStringDefault = """    memset(m_settings.%SettingVarName%, 0, %SettingStringLength%);
    strncpy(m_settings.%SettingVarName%, %SettingDefault%, %SettingStringLength%);
"""

SetArrayDefault = """    memset(m_settings.%SettingVarName%, 0, %SettingSize%);
    memcpy(m_settings.%SettingVarName%, %SettingDefault%, %SettingSize%);
"""

WinIfDef = "defined(_WIN32)\n"
AndroidIfDef = "defined(__ANDROID__)\n"
LnxIfDef = "(__unix__)\n"

ReadSettingsFunc = """
// =====================================================================================================================
%ReadSettingsDesc%
void %ClassName%::%ReadSettingsName%()
{
    // read from the OS adapter for each individual setting
%ReadSettingsCode%
}
"""

PalReadSettingClass = "static_cast<Pal::Device*>(m_pDevice)"
ReadSetting = """    %ReadSettingClass%->ReadSetting(%SettingStrName%,
                           %SettingRegistryType%,
                           &m_settings.%SettingVarName%%OsiSettingType%);
"""

ReadSettingStr = """    %ReadSettingClass%->ReadSetting(%SettingStrName%,
                           %SettingRegistryType%,
                           &m_settings.%SettingVarName%%OsiSettingType%,
                           %StringLength%);
"""
PalOsiSettingType = """,
                           InternalSettingScope::%OsiSettingType%"""

SettingHashListName = "g_%LowerCamelComponentName%SettingHashList"
SettingNumSettingsName = "g_%LowerCamelComponentName%NumSettings"
SettingHashList = """
constexpr const SettingNameHash %SettingHashListName%[] = {
%SettingHashList%};
constexpr uint32 %SettingNumSettingsName% = sizeof(%SettingHashListName%) / sizeof(SettingNameHash);
"""

InitSettingsInfoFunc = """
// =====================================================================================================================
// Initializes the SettingInfo hash map and array of setting hashes.
void %ClassName%::InitSettingsInfo()
{
    SettingInfo info = {};
%InitSettingInfoCode%
}
"""

InitSettingInfo = """
    info.type      = %DevDriverType%;
    info.pValuePtr = &m_settings.%SettingVarName%;
    info.valueSize = sizeof(m_settings.%SettingVarName%);
    m_settingsInfoMap.Insert(%HashName%, info);
"""

JsonDataArray = """
constexpr uint8 %JsonDataArrayName%[] = {
%JsonArrayData%
};  // %JsonDataArrayName%[]
"""

DevDriverRegisterFunc = """
// =====================================================================================================================
// Registers the core settings with the Developer Driver settings service.
void %ClassName%::DevDriverRegister()
{
    auto* pRpcSettingsService = static_cast<Pal::Device*>(m_pDevice)->GetPlatform()->GetSettingsService();
    if (pRpcSettingsService != nullptr)
    {
        RegisteredComponent component = {};
        strncpy(&component.componentName[0], m_pComponentName, kMaxComponentNameStrLen);
        component.pPrivateData = static_cast<void*>(this);
        component.pSettingsHashes = &%SettingHashListName%[0];
        component.numSettings = %SettingNumSettingsName%;
        component.pfnGetValue = ISettingsLoader::GetValue;
        component.pfnSetValue = ISettingsLoader::SetValue;
        component.pSettingsData = &%JsonDataArrayName%[0];
        component.settingsDataSize = sizeof(%JsonDataArrayName%);
        component.settingsDataHash = %SettingsDataHash%;
        component.settingsDataHeader.isEncoded = %IsJsonEncoded%;
        component.settingsDataHeader.magicBufferId = %MagicBufferId%;
        component.settingsDataHeader.magicBufferOffset = %MagicBufferOffset%;

        pRpcSettingsService->RegisterComponent(component);
    }

    auto* pDevDriverServer = static_cast<Pal::Device*>(m_pDevice)->GetPlatform()->GetDevDriverServer();
    if (pDevDriverServer != nullptr)
    {
        auto* pSettingsService = pDevDriverServer->GetSettingsService();
        if (pSettingsService != nullptr)
        {
            RegisteredComponent component = {};
            strncpy(&component.componentName[0], m_pComponentName, kMaxComponentNameStrLen);
            component.pPrivateData = static_cast<void*>(this);
            component.pSettingsHashes = &%SettingHashListName%[0];
            component.numSettings = %SettingNumSettingsName%;
            component.pfnGetValue = ISettingsLoader::GetValue;
            component.pfnSetValue = ISettingsLoader::SetValue;
            component.pSettingsData = &%JsonDataArrayName%[0];
            component.settingsDataSize = sizeof(%JsonDataArrayName%);
            component.settingsDataHash = %SettingsDataHash%;
            component.settingsDataHeader.isEncoded = %IsJsonEncoded%;
            component.settingsDataHeader.magicBufferId = %MagicBufferId%;
            component.settingsDataHeader.magicBufferOffset = %MagicBufferOffset%;

            pSettingsService->RegisterComponent(component);
        }
    }
}
"""
