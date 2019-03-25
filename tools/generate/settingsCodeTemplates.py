##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
CopyrightFilePath = os.path.dirname(os.path.realpath(__file__)) + "/../pal-copyright-template.txt"
FileHeaderCopyright = open(CopyrightFilePath, 'r').read()

FileHeaderWarning = "\
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n\
//\n\
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!\n\
//\n\
// This code has been generated automatically. Do not hand-modify this code.\n\
//\n\
// When changes are needed, modify the tools generating this module in the tools\\generate directory OR settings.cfg\n\
//\n\
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!\n\
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n\
\n"

CopyrightAndWarning = FileHeaderCopyright + FileHeaderWarning

HeaderFileDoxComment = "\n\
/**\n\
***************************************************************************************************\n\
* @file  %FileName%\n\
* @brief auto-generated file.\n\
*        Contains the definition for the PAL settings struct and enums for initialization.\n\
***************************************************************************************************\n\
*/\n\
#pragma once\n"

NamespaceStart = "\nnamespace Pal\n{\n"
HwlNamespaceStart = "namespace %Hwl%\n{\n"
HwlNamespaceEnd   = "\n} // %Hwl%"
NamespaceEnd   = "\n} // Pal"

HeaderIncludes = "\n\
#include \"pal.h\"\n\
#include \"palSettingsLoader.h\"\n"

CppIncludes = "#include \"core/device.h\"\n\
#include \"core/settingsLoader.h\"\n"

IncludeDir = "core/"
HwlIncludeDir = "core/hw/gfxip/%Hwl%/"

PrefixName = ""

DevDriverIncludes = "\n\
#include \"devDriverServer.h\"\n\
#include \"protocols/ddSettingsService.h\"\n\
\n\
using namespace DevDriver::SettingsURIService;\n\
\n"

Enum = "\n\
enum %EnumName% : %EnumDataType%\n\
{\n\
%EnumData%\n\
};\n"

StructDef = "\n\
/// Pal auto-generated settings struct\n\
struct %SettingStructName% : public Pal::DriverSettings\n\
{\n\
%SettingDefs%\
};\n"

SettingDef = "    %SettingType%    %SettingVarName%%ArrayLength%;\n"
SettingStructName = "%UpperCamelComponentName%Settings"
SettingStructDef = "\
    struct {\n\
%StructSettingFields%\
    } %StructSettingName%;\n"

SettingStr = "static const char* %SettingStrName% = %SettingString%;\n"

SetupDefaultsFunc = "\n\
// =====================================================================================================================\n\
// Initializes the settings structure to default values.\n\
void %ClassName%::SetupDefaults()\n\
{\n\
    // set setting variables to their default values...\n\
%SetDefaultsCode%\n\
}\n"

IfMinMax = "#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= %MinVersion% && PAL_CLIENT_INTERFACE_MAJOR_VERSION <= %MaxVersion%\n"
IfMin    = "#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= %MinVersion%\n"
IfMax    = "#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= %MaxVersion%\n"
EndIf    = "#endif\n"

SetDefault = "    m_settings.%SettingVarName% = %SettingDefault%;\n"
SetStringDefault = "    memset(m_settings.%SettingVarName%, 0, %SettingStringLength%);\n\
    strncpy(m_settings.%SettingVarName%, %SettingDefault%, %SettingStringLength%);\n"

SetArrayDefault = "    memset(m_settings.%SettingVarName%, 0, %SettingSize%);\n\
    memcpy(m_settings.%SettingVarName%, %SettingDefault%, %SettingSize%);\n"

WinIfDef = "defined(_WIN32)\n"
LnxIfDef = "(__unix__)\n"

ReadSettingsFunc = "\n\
// =====================================================================================================================\n\
%ReadSettingsDesc%\
void %ClassName%::%ReadSettingsName%()\n\
{\n\
    // read from the OS adapter for each individual setting\n\
%ReadSettingsCode%\n\
}\n"

PalReadSettingClass = "static_cast<Pal::Device*>(m_pDevice)"
ReadSetting = "    %ReadSettingClass%->ReadSetting(%SettingStrName%,\n\
                           %SettingRegistryType%,\n\
                           &m_settings.%SettingVarName%%OsiSettingType%);\n\n"
ReadSettingStr = "    %ReadSettingClass%->ReadSetting(%SettingStrName%,\n\
                           %SettingRegistryType%,\n\
                           &m_settings.%SettingVarName%%OsiSettingType%,\n\
                           %StringLength%);\n\n"
PalOsiSettingType = ",\n                           InternalSettingScope::%OsiSettingType%"

SettingHashListName = "g_%LowerCamelComponentName%SettingHashList"
SettingNumSettingsName = "g_%LowerCamelComponentName%NumSettings"
SettingHashList = "\n\
static const uint32 %SettingNumSettingsName% = %NumSettings%;\n\
static const SettingNameHash %SettingHashListName%[] = {\n\
%SettingHashList%\
};\n"

InitSettingsInfoFunc = "\n\
// =====================================================================================================================\n\
// Initializes the SettingInfo hash map and array of setting hashes.\n\
void %ClassName%::InitSettingsInfo()\n\
{\n\
    SettingInfo info = {};\n\
%InitSettingInfoCode%\n\
}\n"

InitSettingInfo = "\n\
    info.type      = %DevDriverType%;\n\
    info.pValuePtr = &m_settings.%SettingVarName%;\n\
    info.valueSize = sizeof(m_settings.%SettingVarName%);\n\
    m_settingsInfoMap.Insert(%HashName%, info);\n"

JsonDataArray = "\n\
static const uint8 %JsonDataArrayName%[] = {\n\
%JsonArrayData%\n\
};  // %JsonDataArrayName%[]\n"

DevDriverRegisterFunc = "\n\
// =====================================================================================================================\n\
// Registers the core settings with the Developer Driver settings service.\n\
void %ClassName%::DevDriverRegister()\n\
{\n\
    auto* pDevDriverServer = static_cast<Pal::Device*>(m_pDevice)->GetPlatform()->GetDevDriverServer();\n\
    if (pDevDriverServer != nullptr)\n\
    {\n\
        auto* pSettingsService = pDevDriverServer->GetSettingsService();\n\
        if (pSettingsService != nullptr)\n\
        {\n\
            RegisteredComponent component = {};\n\
            strncpy(&component.componentName[0], m_pComponentName, kMaxComponentNameStrLen);\n\
            component.pPrivateData = static_cast<void*>(this);\n\
            component.pSettingsHashes = &%SettingHashListName%[0];\n\
            component.numSettings = %SettingNumSettingsName%;\n\
            component.pfnGetValue = ISettingsLoader::GetValue;\n\
            component.pfnSetValue = ISettingsLoader::SetValue;\n\
            component.pSettingsData = &%JsonDataArrayName%[0];\n\
            component.settingsDataSize = sizeof(%JsonDataArrayName%);\n\
            component.settingsDataHash = %SettingsDataHash%;\n\
            component.settingsDataHeader.isEncoded = %IsJsonEncoded%;\n\
            component.settingsDataHeader.magicBufferId = %MagicBufferId%;\n\
            component.settingsDataHeader.magicBufferOffset = %MagicBufferOffset%;\n\
\n\
            pSettingsService->RegisterComponent(component);\n\
        }\n\
    }\n\
}\n"
