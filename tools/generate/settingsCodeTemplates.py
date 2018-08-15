##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

PalHeaderIncludes = "\n\
#include \"pal.h\"\n\
#include \"palSettingsLoader.h\"\n"

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
SettingStructDef = "\
    struct {\n\
%StructSettingFields%\
    } %StructSettingName%;\n"

StructDefHwl = "\n\
struct %Hwl%%SettingStructName% : public %SettingStructName%\n\
{\n\
%SettingDefs%\n\
};\n\n"

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
// Reads the setting from the OS adapter and sets the structure value when the setting values are found.\n\
void %ClassName%::ReadSettings()\n\
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

GetHwlSettingDecl = "\n\
namespace Pal { class Device; }\n\
extern const Pal::%Hwl%::%Hwl%%SettingStructName%& Get%Hwl%Settings(const Pal::Device& device);\n"

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
            component.settingsDataHeader.isEncoded = %IsJsonEncoded%;\n\
            component.settingsDataHeader.magicBufferId = %MagicBufferId%;\n\
            component.settingsDataHeader.magicBufferOffset = %MagicBufferOffset%;\n\
\n\
            pSettingsService->RegisterComponent(component);\n\
        }\n\
    }\n\
}\n"

SettingsBlockComment = "\n\n\
/**\n\
************************************************************************************************************************\n\
* @page Settings Settings Overview\n\
*\n\
* PAL uses python scripts to generate C++ settings structs and default values from JSON files located in the PAL and\n\
* client source trees. At runtime, the auto-generated functions are called to setup default values and then overrides\n\
* are read from the registry (Windows only) or settings file (Linux). In PAL settings are divided into groups based on\n\
* the scope of the settings. Settings that are only required by the client are called private driver settings. PAL has\n\
* no visibility into these settings and the client is responsible for management of these settings. PAL does, however,\n\
* provide utility classes to help read overrides from the Windows registry or a settings config file (winRegistry.h and\n\
* palSettingsFileMgr.h). Clients can also look at the SettingsLoader class as a model for creating their own settings\n\
* code. \n\
*\n\
* Catalyst system-wide settings can be read by PAL. Only two Catalyst settings (CatalystAI & TFQ) are currently read by\n\
* PAL. These settings are only separated from other settings registry location they are read from.\n\
*\n\
* PAL settings that are internal to PAL and not visible to client code are PAL private settings. These are settings\n\
* that the client is not expected to need to override at runtime. They can, however, still be temporarily modified by\n\
* clients via the registry or settings file.\n\
*\n\
* Settings Config File Format\n\
* ===========================\n\
* PAL uses config files to generate settings structs, default values and provide descriptions for each setting it\n\
* manages. The config file is organized into sections defining enums and their values, nodes which are named groups of\n\
* related settings which contain leafs which are the settings themselves. There is no specific order required for enum\n\
* definitions, nodes or leafs and it is trivial to sort this data in the scripts which generate the C++ code.\n\
*\n\
* Comments\n\
* --------\n\
* Comments are any lines that start with the # character. All text following the # will be ignored by the config parser.\n\
* Enum\n\
* ----\n\
* The following is an example of an enum definition:\n\
* \n\
*<blockquote><pre>DefineEnum = \"'InternalSettingScope' : ('PrivateDriverKey',  '0x0'),\n\
*                               ('PrivatePalKey',     '0x1'),\n\
*                               ('PrivatePalGfx6Key', '0x2'),\n\
*                               ('PublicCatalystKey', '0x3')\";</pre></blockquote>\n\
*\n\
* The enum definition starts with the **DefineEnum** keyword and an equal sign. On the right side of the equal sign the\n\
* enum definition is contained within double quotes and must end with a semicolon. The enum name is enclosed in single\n\
* quotes followed by a colon then each enum member is defined inside of parentheses with a comma separating each\n\
* member. Inside the parens is the value name and numeric value (can be hex or decimal) both enclosed in single quotes\n\
* and separated by a comma.\n\
*\n\
* Node\n\
* ----\n\
* Nodes start with with the Node keyword followed by an equal sign. The name of the node is enclosed in double quotes\n\
* and the contents of the Node are contained within curly brackets {}. No semicolon is required to mark the end of a\n\
* Node. The Node definition can span multiple lines to improve readability of the config file.\n\
*\n\
* Example:\n\
* \n\
*<blockquote><pre>Node = \"General\"\n\
*{\n\
*    Leaf\n\
*    {\n\
*        ...\n\
*     }\n\
*}</pre></blockquote>\n\
*\n\
* Leaf\n\
* ----\n\
* A Leaf definition starts with the Leaf keyword followed by an equal sign. The contents of the leaf are enclosed in\n\
* curly brackets {}. There are several fields which may be specified in the Leaf, each field has a keyword, an equal\n\
* sign and the field contents are surrounded by double quotes. Fields may span multiple lines and each field is\n\
* terminated with a semicolon. The following are the fields currently supported by the config parser along with a\n\
* description of their intended use:\n\
*\n\
* * SettingName - The textual name of the setting being specified\n\
* * SettingType - The Windows registry variable type the setting will use.\n\
* * Description - Text description of the setting.\n\
* * VariableName - The name of setting structure field the setting will use.\n\
* * VariableType - The C++ type that will be used to define the setting in the settings struct.\n\
* * VariableDefault - Default value for the setting struct field. Note, in PAL this value string is copied verbatim to\n\
*                     the setting SetupDefaults function so any valid C++ expression is allowed. For example, a uint32\n\
*                     value may have a VariableDefault of \"64 * 1024\".\n\
* * VariableDefaultWin & VariableDefaultLnx - Setting default value for settings which may have different default values\n\
*                                             based on OS such as a directory string.\n\
* * StringLength - Max allowable length for a string setting type\n\
*\n\
* Example:\n\
*\n\
*<blockquote><pre>Leaf\n\
*{\n\
*   SettingName = \"UseGraphicsCopy\";\n\
*    SettingType = \"BOOL_STR\";\n\
*    VariableName = \"useGraphicsCopy\";\n\
*    Description = \"When true RPM will use the graphics copy path if possible. When false\n\
*                   the compute path will be preferred.\";\n\
*    VariableType = \"bool\";\n\
*    VariableDefault = \"true\";\n\
*}</pre></blockquote>\n\
* \n\
* In addition to the fields listed, the configParser.py python script computes a FNV1a hash of the setting name for\n\
* each Leaf. This hash can be used to provide some rudimentary obfuscation of setting names in the Windows registry to\n\
* avoid accidentally exposing settings to 3rd parties or end users.\n\
************************************************************************************************************************\n\
**/\n"

