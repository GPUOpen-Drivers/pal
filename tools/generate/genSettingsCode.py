##
 #######################################################################################################################
 #
 #  Copyright (c) 2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
import glob
import re
import sys
import time
from configParser import *
from settingsCodeTemplates import *

def getOsiSettingType(settingScope):
    ret = "OsiSettingPrivate"
    if settingScope == "PublicCatalystKey":
        ret = "OsiSettingPublic"
    return ret

def genDoxygenComment(description):
    # Generate a doxygen comment that's limited to 100 columns wide
    newLine = "    /// "
    ret = newLine
    charCount = len(newLine)
    # convert any carriage return/linefeed control chars from the description to ordinary spaces
    description = re.sub(r'\\r\\n', ' ', description)
    for word in description.split():
        charCount += len(word) + 1
        if charCount > 120:
            ret += "\n" + newLine
            charCount = len(newLine)
        ret += " " + word
    return ret + "\n"

Usage = sys.argv[0] + " <Input Config File> <Output Dir> <Output File Name> <hash algorithm [optional]>"

if len(sys.argv) < 4:
    errorExit(Usage)

configFile   = sys.argv[1]
outputDir    = sys.argv[2]
outputFile   = sys.argv[3]
hashAlgorithm = 0

if len(sys.argv) >= 5:
    hashAlgorithm = int(sys.argv[4])

if os.path.exists(configFile) == False:
    errorExit("Config file not found")
if os.path.exists(outputDir) == False:
    errorExit("Output directory not found: " + outputDir)

if outputDir[-1] != "/":
    outputDir = outputDir + "/"

# Before we bother parsing the config file, make sure we can open and write to the output files, the open calls will
# throw an exception and exit if the open fails
headerFileName = outputFile + ".h"
sourceFileName = outputFile + ".cpp"
headerFile     = open(outputDir+headerFileName, 'w')
sourceFile     = open(outputDir+sourceFileName, 'w')

configData = parseConfigFile2(configFile, hashAlgorithm)

# Setup the Enum code
enumCode = ""
for e in configData[Enums]:
    enumData = ""
    for i in range(0, len(e[EnumData][EnumValueName])):
        enumData += "    " + e[EnumData][EnumValueName][i] + " = " + e[EnumData][EnumValue][i] + ",\n"
    enumDef = Enum.replace("%EnumDataType%", "uint32")
    enumDef = enumDef.replace("%EnumName%", e[EnumName])
    enumDef = enumDef.replace("%EnumData%", enumData)
    enumCode += enumDef

# Loop through each setting to create the struct member and settings string for each
settingDefs     = ""
settingsStrings = "\n"
currNode = ""
setDefaultsCode = ""
readSettingsCode = ""
copySettingsCode = ""
updateSettingsCode = ""
for setting in configData[Settings]:
    if currNode != setting[SettingNode]:
        nodeComment = ""
        settingDefs       += nodeComment
        setDefaultsCode   += nodeComment
        readSettingsCode  += nodeComment
        currNode = setting[SettingNode]

    # Do some sanity checking of the setting data
    if setting[SettingVarName] != "":
        if setting[SettingVarType] == "":
            errorExit("Missing Variable Type for " + setting[SettingName])
        # Non string types need an explicit default
        if setting[SettingType] != "STRING" and setting[SettingType] != "STRING_DIR":
            if setting[SettingDefault] == "":
                errorExit("Missing Setting Default Value for " + setting[SettingName])
        else:
            if setting[SettingType] == "STRING_DIR":
                # OS Strings need
                if setting[SettingWinDefault] == "" and setting[SettingLnxDefault] == "":
                    errorExit("Missing OS Specific Default Value for " + setting[SettingName])
            if setting[SettingStringLength] == "":
                errorExit("Missing String Length for " + setting[SettingName])

        # Struct member
        settingDefTmp = SettingDef.replace("%SettingType%", setting[SettingVarType])
        settingDefTmp = settingDefTmp.replace("%SettingVarName%", setting[SettingVarName])
        charArrayLength = ""
        if setting[SettingType]== "STRING" or setting[SettingType]== "STRING_DIR":
            charArrayLength = "[" + setting[SettingStringLength] + "]"
        settingDefTmp = settingDefTmp.replace("%CharArrayLength%", charArrayLength)

        hasIfMin = setting[SettingMinVersion] != ""
        hasIfMax = setting[SettingMaxVersion] != ""
        ifDefTmp   = ""
        endDefTmp  = ""

        # Setup backwards compatibility.
        if hasIfMax or hasIfMin:
            if hasIfMax and hasIfMin:
                ifDefTmp = IfMinMax.replace("%MinVersion%", setting[SettingMinVersion]).replace("%MaxVersion%", setting[SettingMaxVersion])
            elif hasIfMax:
                ifDefTmp = IfMax.replace("%MaxVersion%", setting[SettingMaxVersion])
            elif hasIfMin:
                ifDefTmp = IfMin.replace("%MinVersion%", setting[SettingMinVersion])

            endDefTmp = EndIf

        settingDefs += ifDefTmp
        settingDefs += settingDefTmp
        settingDefs += endDefTmp

        # Set Defaults
        setDefaultTmp = ""
        if setting[SettingType] == "STRING":
            setDefaultTmp = SetStringDefault.replace("%SettingDefault%", "\""+setting[SettingDefault]+"\"")
            setDefaultTmp = setDefaultTmp.replace("%SettingStringLength%", setting[SettingStringLength])
        elif setting[SettingType] == "STRING_DIR":
            setDefaultTmp = SetOsStringDefault.replace("%SettingDefaultWin%", "\""+setting[SettingWinDefault]+"\"")
            setDefaultTmp = setDefaultTmp.replace("%SettingDefaultLnx%", "\""+setting[SettingLnxDefault]+"\"")
            setDefaultTmp = setDefaultTmp.replace("%SettingStringLength%", setting[SettingStringLength])
        else:
            setDefaultTmp = SetDefault.replace("%SettingDefault%", setting[SettingDefault])
        setDefaultTmp = setDefaultTmp.replace("%SettingVarName%", setting[SettingVarName])
        setDefaultTmp = setDefaultTmp.replace("%Hwl%", configData[Hwl])
        setDefaultsCode += ifDefTmp
        setDefaultsCode += setDefaultTmp
        setDefaultsCode += endDefTmp

        # Only settings with a SettingName go into the registry and so only those need a string defined
        # or to have their setting read
        if setting[SettingName] != "":
            # Setting String
            settingStringName = "p" + setting[SettingName] + "Str"
            settingsStringTmp = SettingStr.replace("%SettingStrName%", settingStringName)
            settingString = "\"" + setting[SettingName] + "\""
            if setting[SettingHash] != "":
                settingString = "\"" + str(setting[SettingHash]) + "\""
            settingsStringTmp = settingsStringTmp.replace("%SettingString%", settingString)
            settingsStrings += ifDefTmp
            settingsStrings += settingsStringTmp
            settingsStrings += endDefTmp

            # Read Settings
            readSettingTmp = ReadSetting
            if setting[SettingType] == "STRING" or setting[SettingType] == "STRING_DIR":
                readSettingTmp = ReadSettingStr.replace("%StringLength%", setting[SettingStringLength])
            readSettingTmp = readSettingTmp.replace("%SettingStrName%", settingStringName)
            readSettingClass = ""
            readSettingClass = PalReadSettingClass
            osiSettingTypeTmp = PalOsiSettingType.replace("%OsiSettingType%", setting[SettingScope])
            readSettingTmp = readSettingTmp.replace("%OsiSettingType%", osiSettingTypeTmp)
            readSettingTmp = readSettingTmp.replace("%ReadSettingClass%", readSettingClass)
            readSettingTmp = readSettingTmp.replace("%SettingRegistryType%", setting[SettingRegistryType])
            readSettingTmp = readSettingTmp.replace("%SettingVarName%", setting[SettingVarName])

            readSettingsCode += ifDefTmp
            readSettingsCode += readSettingTmp
            readSettingsCode += endDefTmp

settingStruct = ""
settingStructName = ""
settingStructName = "PalSettings"
if configData[Hwl] == "":
    settingStruct = StructDef.replace("%SettingStructName%", settingStructName)
else:
    settingStruct = StructDefHwl.replace("%SettingStructName%", settingStructName)
    settingStruct = settingStruct.replace("%Hwl%", configData[Hwl])
settingStruct = settingStruct.replace("%SettingDefs%", settingDefs)

includeFileList = ""
headerIncludeList = ""
if configData[Hwl] != "":
    includeFileList = "\n#include \"core/hw/gfxip/" + configData[Hwl].lower() + "/"
    includeFileList += configData[Hwl].lower() + "SettingsLoader.h\"\n"
    # We make the assumption here that the naming convention is consistent for HWL. The base file name should
    # be the same as the HWL name minus the HWL tag.
    # e.g. g_gfx6PalSettings.h will have a base class file named g_palSettings.h
    baseFileName = headerFileName.replace(configData[Hwl].lower() + "Pal", "pal")
    headerIncludeList += "#include \"core/" + baseFileName + "\"\n"
    includeFileList += "#include \"core/hw/gfxip/" + configData[Hwl].lower() + "/" + headerFileName + "\"\n"
    includeFileList += "#include \"palInlineFuncs.h\"\n"
    includeFileList += "#include \"core/device.h\"\n"
else:
    headerIncludeList = PalHeaderIncludes
    includeFileList   = "\n#include \"core/device.h\"\n"
    includeFileList  += "\n#include \"core/settingsLoader.h\"\n"
    includeFileList  += "\n#include \"core/" + headerFileName + "\"\n"
    includeFileList  += "\n#include \"palInlineFuncs.h\"\n"

className = "SettingsLoader"

setupDefaults = SetupDefaultsFunc.replace("%Hwl%", configData[Hwl])
setupDefaults = setupDefaults.replace("%ClassName%", className)
setupDefaults = setupDefaults.replace("%SettingStructName%", settingStructName)
setupDefaults = setupDefaults.replace("%SetDefaultsCode%", setDefaultsCode)

getHwlSettings  = ""
getHwlSettingDecl = ""
if configData[Hwl] != "":
    getHwlSettings = GetHwlSettingFunc.replace("%Hwl%", configData[Hwl])
    getHwlSettings = getHwlSettings.replace("%SettingStructName%", settingStructName)
    getHwlSettingDecl = GetHwlSettingDecl.replace("%Hwl%", configData[Hwl])
    getHwlSettingDecl = getHwlSettingDecl.replace("%SettingStructName%", settingStructName)

readSettings = ReadSettingsFunc.replace("%ClassName%", className)
readSettings = readSettings.replace("%Hwl%", configData[Hwl])
readSettings = readSettings.replace("%SettingStructName%", settingStructName)
readSettings = readSettings.replace("%ReadSettingsCode%", readSettingsCode)

headerDoxComment = HeaderFileDoxComment.replace("%FileName%", outputFile+".h")
copyrightAndWarning = CopyrightAndWarning.replace("%Year%", time.strftime("%Y"))

namespaceStart = "\nnamespace Pal\n{\n"
namespaceEnd   = "\n} // Pal"
if configData[Hwl] != "":
    namespaceStart += "namespace " + configData[Hwl] + "\n{\n"
    namespaceEnd   = "\n} // " + configData[Hwl] + namespaceEnd

######### Build the Header File(s) ################
headerFileTxt  = copyrightAndWarning + headerDoxComment + headerIncludeList + namespaceStart + enumCode
headerFileTxt += settingStruct + settingsStrings + namespaceEnd + getHwlSettingDecl
headerFile.write(headerFileTxt)
headerFile.close()

######### Build the Source File ################
sourceFileTxt = copyrightAndWarning + includeFileList + namespaceStart + setupDefaults
sourceFileTxt += readSettings + namespaceEnd + getHwlSettings
sourceFile.write(sourceFileTxt)
sourceFile.close()

print("Finished generating " + outputFile)

sys.exit(0)
