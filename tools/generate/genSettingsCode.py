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
import glob
import re
import sys
import time
import json
import textwrap
from settingsCodeTemplates import *

# 4 space indentation
indt = "    "
trailComment = "  //< "
comment = "// "
newline = "\n"

def getOsiSettingType(settingScope):
    ret = "OsiSettingPrivate"
    if settingScope == "PublicCatalystKey":
        ret = "OsiSettingPublic"
    return ret

def genComment(description, indent):
    # Generate a comment that's limited to 120 columns wide
    startOfLine = indent + comment
    ret = startOfLine
    charCount = len(startOfLine)
    # convert any carriage return/linefeed control chars from the description to ordinary spaces
    description = re.sub(r'\\r\\n', ' ', description)
    for word in description.split():
        charCount += len(word) + 1
        if charCount > 120:
            ret += "\n" + startOfLine
            charCount = len(startOfLine)
        ret += " " + word
    return ret

def genDefaultLine(defaultValue, varName, isString, isHex, size):
    default = ""
    defaultValueStr = str(defaultValue)
    if type(defaultValue) is bool:
        defaultValueStr = defaultValueStr.lower()
    elif isHex:
        try:
            defaultValueStr = hex(defaultValue)
        except TypeError:
            print("Couldn't convert setting " + varName + " to hex format.")
            defaultValueStr = str(defaultValue)


    if isString:
      defaultValueStr = "\""+defaultValue.replace('\\', '\\\\')+"\""
      default = SetStringDefault.replace("%SettingStringLength%", str(size))
    elif size != "":
      default = SetArrayDefault.replace("%SettingSize%", str(size))
    else:
      default = SetDefault

    default = default.replace("%SettingDefault%", defaultValueStr)
    default = default.replace("%SettingVarName%", varName)

    return default

maxTypeLen = 30
def getTypeSpacing(type):
    assertExit((len(type) < maxTypeLen), "Need to increase maxTypeLen for type name: " + type)
    numSpaces = maxTypeLen - len(type)
    return " " * numSpaces

registryTypes = { "uint32":"Uint",
                  "bool":"Boolean",
                  "int32":"Int",
                  "gpusize":"Uint64",
                  "float":"Float",
                  "string":"Str",
                  "uint64":"Uint64",
                  "size_t":"Uint",
                  "enum": "Uint" }
def getRegistryType(type):
    return "Util::ValueType::"+registryTypes[type]

ddSettingTypes = { "uint32":"Uint",
                   "bool":"Boolean",
                   "int32":"Int",
                   "gpusize":"Uint64",
                   "float":"Float",
                   "string":"String",
                   "uint64":"Uint64",
                   "size_t":"Uint",
                   "enum": "Uint" }
def getDevDriverType(type):
    return "SettingType::"+ddSettingTypes[type]

def assertExit(condition, msg):
    if not condition:
        errorExit(msg)

def errorExit(msg):
    print("ERROR: " + msg)
    sys.exit(1)

def fnv1a(str):
    fnv_prime = 0x01000193;
    hval      = 0x811c9dc5;
    uint32Max = 2 ** 32;

    for c in str:
        hval = hval ^ ord(c);
        hval = (hval * fnv_prime) % uint32Max;
    return hval

# Encrypt the key string using sha1 secure hash.
def sha1(str):
    m = hashlib.sha1()
    #add random SALT to the text key
    m.update("gqBGG$0$4EX@nPsBuF=|a6uRlBWo@ITpWN8WRGFKWdi7wlw@&AMJxAFWeRc2ls!li*0o#M4z%sj#|V_j".encode())
    m.update(str.encode())
    return m.hexdigest()

def defineEnum(valueList):
    enumDef = ""
    # IsEnum indicates that the valid values should be used to create an enum definition
    if ("IsEnum" in valueList) and valueList["IsEnum"]:
        # This will be the full list of value definitions
        enumData = ""
        # build the enum definition by starting with the list of value definitions
        for i, value in enumerate(valueList["Values"]):
            enumValue = indt + value["Name"] + " = " + str(value["Value"])
            # If this is not the last value add a comma to continue the list
            if i < (len(valueList["Values"]) - 1):
                enumValue += ","
            if "Description" in valueList:
                # We put the description field in as a comment, if it all fits on one line put the comment to the right
                # of the value definition, otherwise put it on a separate line above.
                if (len(enumValue) + len(trailComment) + len(value["Description"])) <= 120:
                    enumValue = enumValue + trailComment + value["Description"]
                else:
                    enumValue = genComment(value["Description"], indt) + newline + enumValue
            # Add this value to the full list, and if it's not the last value add a newline
            enumData += enumValue
            if i < (len(valueList["Values"]) - 1):
                enumData += newline

        # Now fill in the dynamic data in the enum template
        enumDef = Enum.replace("%EnumDataType%", "uint32")
        enumDef = enumDef.replace("%EnumName%", valueList["Name"])
        enumDef = enumDef.replace("%EnumData%", enumData)
    return enumDef

def defineSettingVariable(setting):
    type = setting["Type"]
    if type == "string":
      type = "char"
    elif type == "enum":
      assertExit((("ValidValues" in setting) and ("Name" in setting["ValidValues"])),
                 "Named ValidValues definition missing from Enum type setting: " + setting["Name"])
      type = setting["ValidValues"]["Name"]
    settingDef = SettingDef.replace("%SettingType%", (type + getTypeSpacing(type)))
    settingDef = settingDef.replace("%SettingVarName%", setting["VariableName"])
    arrayLength = ""
    # If the type is string then we need to setup the char array length
    if "Size" in setting:
        arrayLength = "[" + str(setting["Size"]) + "]"
    settingDef = settingDef.replace("%ArrayLength%", arrayLength)
    return settingDef

def genHashedStringName(name, hashName):
    # Setting String definition
    settingStringName = "p" + name + "Str"
    settingsStringTmp = SettingStr.replace("%SettingStrName%", settingStringName)
    settingsStringTmp = settingsStringTmp.replace("%SettingString%", "\"#"+ hashName + "\"")
    return settingStringName, settingsStringTmp

def genReadSettingCode(data):

    # First get the hashed name string
    settingStringName, settingStringTmp = genHashedStringName(data["name"], str(data["hashName"]))

    readSettingTmp = ReadSetting
    if data["stringLen"] > 0:
        readSettingTmp = ReadSettingStr.replace("%StringLength%", str(data["stringLen"]))
    readSettingTmp = readSettingTmp.replace("%SettingStrName%", settingStringName)
    osiSettingTypeTmp = PalOsiSettingType.replace("%OsiSettingType%", data["scope"])
    readSettingTmp = readSettingTmp.replace("%OsiSettingType%", osiSettingTypeTmp)
    readSettingTmp = readSettingTmp.replace("%ReadSettingClass%", PalReadSettingClass)
    readSettingTmp = readSettingTmp.replace("%SettingRegistryType%", getRegistryType(data["type"]))
    readSettingTmp = readSettingTmp.replace("%SettingVarName%", data["variableName"])

    return settingStringTmp, readSettingTmp

def setupReadSettingData(name, scope, type, varName, size, structName, structVarName, arrayIndex):

    finalName = name
    finalVarName = varName
    # If this is a struct field, the registry name will be StructName_Name and we setup the variable name
    # to reference from the structure
    if structName != "":
        finalName = structName + "_" + finalName
        finalVarName = structVarName + "." + finalVarName
    # If this is an array element then we add the array index to the registry name and setup the variable name
    # to refer to the correct element in the array.
    if arrayIndex != "":
        finalName = finalName + "_" + arrayIndex
        finalVarName = finalVarName + "[" + arrayIndex + "]"

    stringLen = 0
    if type == "string":
        stringLen = size

    return { "name":  finalName,
             "scope": scope,
             "type":  type,
             "stringLen": stringLen,
             "variableName": finalVarName }

def genInitSettingInfoCode(type, varName, hashName):
    initSettingInfoTmp = InitSettingInfo.replace("%DevDriverType%", getDevDriverType(type))
    initSettingInfoTmp = initSettingInfoTmp.replace("%SettingVarName%", varName)
    initSettingInfoTmp = initSettingInfoTmp.replace("%HashName%", str(hashName))
    return initSettingInfoTmp

Usage = sys.argv[0] + " <Input Settings JSON File> <Output Dir> <Output File Name [without g_ or file extension]> <Magic Buffer File (optional)> <Magic Buffer File Offset (optional)>"

assertExit(((len(sys.argv) >= 4) and (len(sys.argv) <= 6)), Usage)

settingsJsonFilename   = sys.argv[1]
outputDir    = sys.argv[2]
outputFile   = "g_" + sys.argv[3]
magicBufferFilename = ""

magicBufferOffset = 0
if len(sys.argv) >= 5:
    magicBufferFilename = sys.argv[4]

if len(sys.argv) >= 6:
    magicBufferOffset = sys.argv[5]

# First make sure the settings input file, encode config file and output directory exist and can be opened.  The open
# calls will throw an exception and exit on a failure.
settingsJsonFile = ""
jsonEncodeConfigFile = ""
settingsJsonStr = ""
jsonEncodeConfigStr = ""

# Try to open and parse the settings JSON data
assertExit(os.path.exists(settingsJsonFilename), "Config file not found")
settingsJsonFile = open(settingsJsonFilename, 'r')
settingsJsonStr = settingsJsonFile.read()

#Try to open the magicBuffer file
magicBufferFile = ""
magicBufferHashName = 0
if magicBufferFilename != "":
    assertExit(os.path.exists(magicBufferFilename), "Magic buffer file not found")
    magicBufferFile = open(magicBufferFilename, 'r')
    magicBufferHashName = fnv1a(os.path.basename(magicBufferFilename))

assertExit(os.path.exists(outputDir), "Output directory not found: " + outputDir)

if outputDir[-1] != "/":
    outputDir = outputDir + "/"

# Make sure we can open and write to the output files.
headerFileName = outputFile + ".h"
sourceFileName = outputFile + ".cpp"
headerFile     = open(outputDir+headerFileName, 'w')
sourceFile     = open(outputDir+sourceFileName, 'w')

# Read/Parse the settings JSON data
settingsData  = json.loads(settingsJsonStr)

enumCode = ""
settingDefs = ""
setDefaultsCode = ""
readSettingsCode = ""
copySettingsCode = ""
updateSettingsCode = ""
settingsStrings = ""
settingHashList = ""
settingInfoCode = ""
numHashes = 0

# Parse the defined constants into it's own dictionary for easier lookup
constantDict = {}
if "DefinedConstants" in settingsData:
    for constant in settingsData["DefinedConstants"]:
        constantDict[constant["Name"]] = constant["Value"]

# Process any top level enum definitions
if "Enums" in settingsData:
    for enum in settingsData["Enums"]:
        enumCode += defineEnum(enum)

for setting in settingsData["Settings"]:

    isString = setting["Type"] == "string"
    isStruct = "Structure" in setting
    isHex = "Flags" in setting and "IsHex" in setting["Flags"] and setting["Flags"]["IsHex"] == True
    # Make sure that the "Structure" object exists for struct types (and is not present for non-structs)
    assertExit(((setting["Type"] != "struct") or isStruct), "Wrong type or Structure defintiion missing for setting: " + setting["Name"])
    # Make sure the "Size" field exists for string types
    assertExit((not(isString) or ("Size" in setting)), "Size missing from string setting: " + setting["Name"])

    ifDefTmp   = ""
    endDefTmp  = ""

    # Check for the size field and convert the constant to an integer value (if necessary) for use in later loops
    settingIntSize = 0
    if "Size" in setting:
        if type(setting["Size"]) is int:
            settingIntSize = setting["Size"]
        else:
            settingIntSize = constantDict[setting["Size"]]

    ###################################################################################################################
    # Setup backwards compatibility.
    ###################################################################################################################
    if "Preprocessor" in setting:
        endifCount = 0
        directives = setting["Preprocessor"]
        hasMax = ("MaxVersion" in directives)
        hasMin = ("MinVersion" in directives)
        if hasMin and hasMax:
            ifDefTmp = IfMinMax.replace("%MinVersion%", directives["MinVersion"]).replace("%MaxVersion%", directives["MaxVersion"])
            endifCount += 1
        elif hasMax:
            ifDefTmp = IfMax.replace("%MaxVersion%", str(directives["MaxVersion"]))
            endifCount += 1
        elif hasMin:
            ifDefTmp = IfMin.replace("%MinVersion%", str(directives["MinVersion"]))
            endifCount += 1

        if "Custom" in directives:
            for d in directives["Custom"]:
                ifDefTmp += d["Command"] + " " + d["Expression"] + "\n"
                endifCount += 1

        while endifCount > 0:
            endDefTmp += EndIf
            endifCount -= 1

    ###################################################################################################################
    # Create enum definition from the valid value list (if required)
    ###################################################################################################################
    if "ValidValues" in setting:
        enumCode += defineEnum(setting["ValidValues"])
    elif setting["Type"] == "struct":
        for field in setting["Structure"]:
            if "ValidValues" in field:
                enumCode += defineEnum(field["ValidValues"])

    ###################################################################################################################
    # Define the Setting variable
    ###################################################################################################################
    # For non-struct types the setting definition is straightforward
    if setting["Type"] != "struct":
        settingDefTmp = defineSettingVariable(setting)
    else:
        # struct types require the struct definition and a loop to define each subfield
        settingDefTmp = SettingStructDef.replace("%StructSettingName%", setting["VariableName"])
        structFields = ""
        for field in setting["Structure"]:
            structFields += indt + defineSettingVariable(field)
        settingDefTmp = settingDefTmp.replace("%StructSettingFields%", structFields)

    settingDefs += ifDefTmp
    settingDefs += settingDefTmp
    settingDefs += endDefTmp

    ###################################################################################################################
    # SetDefaults() per setting code
    ###################################################################################################################
    setDefaultTmp = ""

    defaultsCode = ""
    winDefaultsCode = ""
    lnxDefaultsCode = ""

    settingDefaultCode = ""

    isTarget = False
    hasWinDefault = False
    hasLnxDefault = False
    if isStruct:
        # For structs we want to loop through each field and generate the line that sets the default, once for each
        # set of default values specified.
        for field in setting["Structure"]:
            isFieldString = (field["Type"] == "string")
            isFieldHex = "Flags" in field and "IsHex" in field["Flags"] and field["Flags"]["IsHex"] == True
            # Make sure string fields have a size
            assertExit((not(isFieldString) or ("Size" in field)), "Size missing from string field: " + setting["Name"] + "." + field["Name"])

            fieldDefaults = field["Defaults"]
            fieldHasWinDefault = "WinDefault" in fieldDefaults
            fieldHasLnxDefault = "LnxDefault" in fieldDefaults

            size = ""
            if "Size" in field:
                if type(field["Size"]) is int:
                    size = field["Size"]
                else:
                    size = constantDict[field["Size"]]

            fieldVarName = setting["VariableName"] + "." + field["VariableName"]
            defaultsCode += genDefaultLine(fieldDefaults["Default"], fieldVarName, isFieldString, isFieldHex, size)
            if fieldHasWinDefault:
                winDefaultsCode += genDefaultLine(fieldDefaults["WinDefault"], fieldVarName, isFieldString, isFieldHex, size)
                hasWinDefault = True
            if fieldHasLnxDefault:
                lnxDefaultsCode += genDefaultLine(fieldDefaults["LnxDefault"], fieldVarName, isFieldString, isFieldHex, size)
                hasLnxDefault = True
    else:
        defaults = setting["Defaults"]
        hasWinDefault = "WinDefault" in defaults
        hasLnxDefault = "LnxDefault" in defaults

        size = ""
        if "Size" in setting:
            size = settingIntSize

        defaultsCode = genDefaultLine(defaults["Default"], setting["VariableName"], isString, isHex, size)
        if hasWinDefault:
            winDefaultsCode = genDefaultLine(defaults["WinDefault"], setting["VariableName"], isString, isHex, size)
        if hasLnxDefault:
            lnxDefaultsCode = genDefaultLine(defaults["LnxDefault"], setting["VariableName"], isString, isHex, size)

    # If there is a Windows or Linux specific default we have to surround the code in the proper #if blocks
    if hasWinDefault and hasLnxDefault:
        settingDefaultCode = "#if " + WinIfDef + winDefaultsCode
        settingDefaultCode += "#elif " + LnxIfDef + lnxDefaultsCode + EndIf
    elif hasWinDefault:
        settingDefaultCode = "#if " + WinIfDef + winDefaultsCode
        settingDefaultCode += "#else \n" + defaultsCode + EndIf
    elif hasLnxDefault:
        settingDefaultCode = "#if " + LnxIfDef + lnxDefaultsCode
        settingDefaultCode += "#else \n" + defaultsCode + EndIf
    else:
        settingDefaultCode = defaultsCode

    setDefaultsCode += ifDefTmp
    setDefaultsCode += settingDefaultCode
    setDefaultsCode += endDefTmp

    ###################################################################################################################
    # ReadSettings() per setting code
    ###################################################################################################################
    # The presence of the Scope field indicates that the setting can be read from the registry
    if "Scope" in setting:
        readSettingData = []
        if setting["Type"] == "struct":
            # Struct type settings have their fields stored in the registry with the struct name prepended.
            # For example a struct setting named fancyStruct with a field named haxControl would be stored in the
            # registry as: "FancyStruct_HaxControl"
            for field in setting["Structure"]:
                size = ""
                if "Size" in field:
                    if type(field["Size"]) is int:
                        size = field["Size"]
                    else:
                        size = constantDict[field["Size"]]

                # Non array types just need to make a single call to setup the readSetting data
                if (field["Type"] == "string") or (settingIntSize == 0):
                    data = setupReadSettingData(field["Name"],
                                                setting["Scope"],
                                                field["Type"],
                                                field["VariableName"],
                                                size,
                                                setting["Name"],
                                                setting["VariableName"],
                                                "")
                    data["hashName"] = field["HashName"]
                    readSettingData.append(data)
                else:
                    # For arrays we have to loop once for each element
                    for i in range(settingIntSize):
                        data = setupReadSettingData(field["Name"],
                                                    setting["Scope"],
                                                    field["Type"],
                                                    field["VariableName"],
                                                    size,
                                                    setting["Name"],
                                                    setting["VariableName"],
                                                    str(i))
                        data["hashName"] = field["HashName"]
                        readSettingData.append(data)

        elif setting["Type"] == "string":
            data = setupReadSettingData(setting["Name"],
                                        setting["Scope"],
                                        setting["Type"],
                                        setting["VariableName"],
                                        settingIntSize,
                                        "",
                                        "",
                                        "")
            data["hashName"] = setting["HashName"]
            readSettingData.append(data)
        elif settingIntSize > 0:
            # Array types are stored in the registry with each element matching the array name with the element index
            # appended. For example an array setting named "BestSettingEver" with a size of 4 would have its elements
            # stored with the names: "BestSettingEver_0", "BestSettingEver_1", "BestSettingEver_2", "BestSettingEver_3"
            for i in range(settingIntSize):
                data = setupReadSettingData(setting["Name"],
                                            setting["Scope"],
                                            setting["Type"],
                                            setting["VariableName"],
                                            settingIntSize,
                                            "",
                                            "",
                                            str(i))
                data["hashName"] = setting["HashName"]
                readSettingData.append(data)
        else:
            data = setupReadSettingData(setting["Name"],
                                        setting["Scope"],
                                        setting["Type"],
                                        setting["VariableName"],
                                        0,
                                        "",
                                        "",
                                        "")
            data["hashName"] = setting["HashName"]
            readSettingData.append(data)

        for data in readSettingData:
            settingsStringTmp, readSettingTmp = genReadSettingCode(data)

            settingsStrings += ifDefTmp
            settingsStrings += settingsStringTmp
            settingsStrings += endDefTmp

            readSettingsCode += ifDefTmp
            readSettingsCode += readSettingTmp
            readSettingsCode += endDefTmp
    ###################################################################################################################
    # InitSettingsData() per setting code
    ###################################################################################################################
    settingInfoCodeTmp = ""
    if setting["Type"] == "struct":
        for field in setting["Structure"]:
            varName = setting["VariableName"] + "." + field["VariableName"]
            numHashes = numHashes + 1
            settingInfoCode += ifDefTmp
            settingInfoCode += genInitSettingInfoCode(field["Type"], varName, field["HashName"])
            settingInfoCode += endDefTmp

            settingHashList += ifDefTmp
            settingHashList += str(field["HashName"]) + ",\n"
            settingHashList += endDefTmp
    else:
        numHashes = numHashes + 1
        settingInfoCode += ifDefTmp
        settingInfoCode += genInitSettingInfoCode(setting["Type"], setting["VariableName"], setting["HashName"])
        settingInfoCode += endDefTmp

        settingHashList += ifDefTmp
        settingHashList += str(setting["HashName"]) + ",\n"
        settingHashList += endDefTmp

upperCamelComponentName = settingsData["ComponentName"].replace("_", " ")
upperCamelComponentName = "".join(x for x in upperCamelComponentName.title() if not x.isspace())
lowerCamelComponentName = upperCamelComponentName[0].lower() + upperCamelComponentName[1:]

hardwareLayer = ""
if "Hwl" in settingsData:
    hardwareLayer = settingsData["Hwl"]
hardwareLayerLower = hardwareLayer.lower()

###################################################################################################################
# Setup the Setting struct definition
###################################################################################################################
settingStruct = ""
settingStructName = upperCamelComponentName + "Settings"

settingStruct = StructDef.replace("%SettingStructName%", settingStructName)
settingStruct = settingStruct.replace("%SettingDefs%", settingDefs)

###################################################################################################################
# Build the include file lists for header and source files
###################################################################################################################
className = "SettingsLoader"

includeFileList = ""
headerIncludeList = PalHeaderIncludes
if hardwareLayer != "":
    headerIncludeList += "\n#include \"core/hw/gfxip/gfxDevice.h\"\n"
    includeFileList = "\n#include \"core/hw/gfxip/" + hardwareLayerLower + "/"
    includeFileList += hardwareLayerLower + "SettingsLoader.h\"\n"
    includeFileList += "#include \"core/hw/gfxip/" + hardwareLayerLower + "/" + headerFileName + "\"\n"
else:
    includeFileList  += "#include \"core/settingsLoader.h\"\n"
    includeFileList  += "#include \"core/" + headerFileName + "\"\n"
includeFileList  += "#include \"core/device.h\"\n"
includeFileList  += "#include \"palInlineFuncs.h\"\n"
includeFileList  += "#include \"palHashMapImpl.h\"\n"

###################################################################################################################
# Setup the JSON data array
###################################################################################################################
# If a magic buffer file was not provided, store the JSON data as a uint8 array
jsonUintData = ""
isEncoded = "false"
if magicBufferFilename == "":
    paddedUints = []
    for i in range(0, len(settingsJsonStr)):
        paddedUints.append(str(ord(settingsJsonStr[i])))

    jsonUintStr = ", ".join(paddedUints)
    wrapper = textwrap.TextWrapper()
    wrapper.width = 120
    wrapper.initial_indent = "    "
    wrapper.subsequent_indent = "    "
    jsonUintData = "\n".join(wrapper.wrap(jsonUintStr))
else:
    #Otherwise perform one-time pad on the JSON data when creating the array
    magicBuffer = []
    paddedUints = []
    # Read the magic buffer in as an array of integers
    magicBuffer = magicBufferFile.read().split(', ')
    magicBufferFile.close()
    for i in range(0, len(settingsJsonStr)):
        magicBufferIdx = i % len(magicBuffer)
        paddedUints.append(str(ord(settingsJsonStr[i])^int(magicBuffer[magicBufferIdx])))

    paddedJsonStr = ", ".join(paddedUints)
    wrapper = textwrap.TextWrapper()
    wrapper.width = 120
    wrapper.initial_indent = "    "
    wrapper.subsequent_indent = "    "
    jsonUintData = "\n".join(wrapper.wrap(paddedJsonStr))
    isEncoded = "true"
jsonArrayName = "g_" + lowerCamelComponentName +"JsonData"
jsonDataArray = JsonDataArray.replace("%JsonArrayData%", jsonUintData)
jsonDataArray = jsonDataArray.replace("%JsonDataArrayName%", jsonArrayName)

###################################################################################################################
# Finish generating the SetupDefaults(), ReadSettings(), InitSettingsInfo() and DevDriverRegister() functions
###################################################################################################################
settingHashListName = SettingHashListName.replace("%LowerCamelComponentName%", lowerCamelComponentName)
settingNumSettingsName = SettingNumSettingsName.replace("%LowerCamelComponentName%", lowerCamelComponentName)
# add one more line to SetupDefaults to initialize the numSettings field
setDefaultsCode += "    m_settings.numSettings = " + settingNumSettingsName + ";"

setupDefaults = SetupDefaultsFunc.replace("%ClassName%", className)
setupDefaults = setupDefaults.replace("%SettingStructName%", settingStructName)
setupDefaults = setupDefaults.replace("%SetDefaultsCode%", setDefaultsCode)

readSettings = ReadSettingsFunc.replace("%ClassName%", className)
readSettings = readSettings.replace("%SettingStructName%", settingStructName)
readSettings = readSettings.replace("%ReadSettingsCode%", readSettingsCode)

initSettingsInfo = InitSettingsInfoFunc.replace("%ClassName%", className)
initSettingsInfo = initSettingsInfo.replace("%InitSettingInfoCode%", settingInfoCode)

settingHashListCode = SettingHashList.replace("%NumSettings%", str(numHashes))
settingHashListCode = settingHashListCode.replace("%SettingHashList%", settingHashList)
settingHashListCode = settingHashListCode.replace("%SettingHashListName%", settingHashListName)
settingHashListCode = settingHashListCode.replace("%SettingNumSettingsName%", settingNumSettingsName)

devDriverRegister = DevDriverRegisterFunc.replace("%ClassName%", className)
devDriverRegister = devDriverRegister.replace("%SettingHashListName%", settingHashListName)
devDriverRegister = devDriverRegister.replace("%SettingNumSettingsName%", settingNumSettingsName)
devDriverRegister = devDriverRegister.replace("%JsonDataArrayName%", jsonArrayName)
devDriverRegister = devDriverRegister.replace("%IsJsonEncoded%", isEncoded)
devDriverRegister = devDriverRegister.replace("%MagicBufferId%", str(magicBufferHashName))
devDriverRegister = devDriverRegister.replace("%MagicBufferOffset%", str(magicBufferOffset))

headerDoxComment = HeaderFileDoxComment.replace("%FileName%", headerFileName)
copyrightAndWarning = CopyrightAndWarning.replace("%Year%", time.strftime("%Y"))

namespaceStart = "\nnamespace Pal\n{\n"
namespaceEnd   = "\n} // Pal"
if hardwareLayer != "":
    namespaceStart += "namespace " + hardwareLayer + "\n{\n"
    namespaceEnd   = "\n} // " + hardwareLayer + namespaceEnd

###################################################################################################################
# Build the Header File
###################################################################################################################
headerFileTxt  = copyrightAndWarning + headerDoxComment + headerIncludeList + namespaceStart + enumCode
headerFileTxt += settingStruct + settingsStrings + settingHashListCode + jsonDataArray + namespaceEnd
headerFile.write(headerFileTxt)
headerFile.close()

###################################################################################################################
# Build the Source File
###################################################################################################################
sourceFileTxt = copyrightAndWarning + includeFileList + DevDriverIncludes + namespaceStart + setupDefaults
sourceFileTxt += readSettings + initSettingsInfo + devDriverRegister + namespaceEnd
sourceFile.write(sourceFileTxt)
sourceFile.close()

print ("Finished generating " + headerFileName + " and " + sourceFileName)

sys.exit(0)
