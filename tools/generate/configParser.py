##
 ###############################################################################
 #
 # Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
 #
 # Permission is hereby granted, free of charge, to any person obtaining a copy
 # of this software and associated documentation files (the "Software"), to deal
 # in the Software without restriction, including without limitation the rights
 # to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 # copies of the Software, and to permit persons to whom the Software is
 # furnished to do so, subject to the following conditions:
 #
 # The above copyright notice and this permission notice shall be included in
 # all copies or substantial portions of the Software.
 #
 # THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 # IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 # FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 # AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 # LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 # OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 # THE SOFTWARE.
 ##############################################################################/


import os
import glob
import re
import sys
import hashlib

# enum like values for setting up return data structures
## Config Data
Enums               = 0
Nodes               = 1
Settings            = 2
Hwl                 = 3
NodeParentIndices   = 4

## Enum Data
EnumName            = 0
EnumData            = 1
EnumScope           = 2
EnumValueName       = 0
EnumValue           = 1
## Settings Data
SettingName         = 0
SettingNode         = 1
SettingType         = 2
SettingVarName      = 3
SettingDesc         = 4
SettingVarType      = 5
SettingDefault      = 6
SettingWinDefault   = 7
SettingLnxDefault   = 8
SettingScope        = 9
SettingFriendlyName = 10
SettingStringLength = 11
SettingRegistryType = 12
SettingWhitelist    = 13
SettingBlacklist    = 14
SettingHash         = 15
SettingDevDriver    = 16
SettingMinVersion   = 17
SettingMaxVersion   = 18

# Config parse strings
StartEnum         = "DefineEnum"
StartPrivEnum     = "DefinePrivEnum"
StartNode         = "Node"
StartLeaf         = "Leaf"
StartHwl          = "HWL"
StartDefaultScope = "DefaultScope"

# Settings parse strings
SettingParseStrings = {
    SettingName         : "SettingName",
    SettingNode         : "SettingNode",
    SettingType         : "SettingType",
    SettingVarName      : "VariableName",
    SettingDesc         : "Description",
    SettingVarType      : "VariableType",
    SettingDefault      : "VariableDefault",
    SettingWinDefault   : "VariableDefaultWin",
    SettingLnxDefault   : "VariableDefaultLnx",
    SettingScope        : "SettingScope",
    SettingFriendlyName : "FriendlyName",
    SettingStringLength : "StringLength",
    SettingRegistryType : "RegistryType",
    SettingWhitelist    : "ISV_Whitelist",
    SettingBlacklist    : "ISV_Blacklist",
    SettingDevDriver    : "DevDriver",
    SettingMinVersion   : "MinVersion",
    SettingMaxVersion   : "MaxVersion"}

# Registry type lookup table, finds the RegistryType based on SettingType
RegistryTypeTable = {
    "BOOL_STR":   "Util::ValueType::Boolean",
    "UINT_STR":   "Util::ValueType::Uint",
    "INT_STR":    "Util::ValueType::Int",
    "FLOAT_STR":  "Util::ValueType::Float",
    "HEX_STR":    "Util::ValueType::Uint",
    "HEX64_STR":  "Util::ValueType::Uint64",
    "STRING":     "Util::ValueType::Str",
    "STRING_DIR": "Util::ValueType::Str"
}

SettingScopeDefault = ""

# Helper class primarily used to keep track of line numbers during line by line file reading
class FileReader:
    def __init__(self, filename):
        self._file = open(filename, 'r')
        self._lineNumber = 0
        self._filename = filename

    def readLine(self):
        line = self._file.readline()
        self._lineNumber += 1
        return line

    def getLineNumber(self):
        return self._lineNumber

    def getFilename(self):
        return self._filename

    def close(self):
        self._file.close()

def isCfgComment(line):
    if line.strip() == "":
        return False
    else:
        char = line.lstrip()[0]
        return char == "#" or char == "/" or char == "*"

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

# extracts the the text in between the first set of single quotes
# Ex: "This is 'the best' example 'ever!'" would return "the best"
def extractSingleQuotedText(line):
    return line.partition("'")[-1].partition("'")[0]

# same as extractSingleQuotedText except extracts text from double quotes
def extractDoubleQuotedText(line):
    return line.partition('"')[-1].partition('"')[0]

# extracts double quoted text that may be split over multiple lines. The end quote is expected to be followed
# by a semicolon so quotes in the middle of the string are ignored
def extractMultiLineQuotedText(line, fileReader):
    if line.find('";') >= 0:
        # the entire string is on one line, just pass to extractDoubleQuotedText
        return extractDoubleQuotedText(line)
    else:
        # readLine leaves a newline character in the string, but we specify those explicitly so strip the last one off
        retText = line.partition('"')[-1].rpartition("\n")[0]
        while True:
            # subsequent lines may have a bunch of leading space to make the config file look pretty, but we don't
            # want that space in the actual description.
            line = fileReader.readLine().lstrip()
            if line == "":
                errorExit("End of file found when extracting multiline quoted text: %s:%d" %
                          (fileReader.getFilename(), fileReader.getLineNumber()))

            if line.find('";') >= 0:
                # this is the final line, just take everything up to the final quote
                if line.find('"') == -1:
                    errorExit("Missing final quote in multiline quoted text: %s:%d" %
                              (fileReader.getFilename(), fileReader.getLineNumber()))
                retText = retText + line.rpartition('"')[0]
                break
            else:
                retText = retText + line.rpartition("\n")[0]
        return retText

def parseEnum(line, fileReader):
    # tokens are enclosed in single quotes, the first line contains the enum name & first enum name/value pair
    enumName = extractSingleQuotedText(line)
    enumValueNames = []
    enumValues     = []
    enumScope      = "public"
    if line.find(StartPrivEnum) >= 0:
        enumScope = "private"
    # strip off everything up to and including the : and then start extracting names and values
    line = line.partition(":")[-1]
    while True:
        enumValueNames.append(extractSingleQuotedText(line))
        enumValues.append(extractSingleQuotedText(line.partition(",")[-1]))
        if line == "":
            errorExit("Found EOF while parsing enum %s: %s:%d" %
                      (enumName, fileReader.getFilename(), fileReader.getLineNumber()))
        elif line.find(";") >= 0:
            # we've reached the end of the enum definition, break out of this loop
            break
        else:
            #otherwise read the next line and continue
            line = fileReader.readLine()
    return {EnumName: enumName, EnumData: {EnumValueName: enumValueNames, EnumValue: enumValues}, EnumScope: enumScope}

def parseLeaf(line, fileReader, hashAlgorithm):
    settingData = { SettingName         : "",
                    SettingType         : "",
                    SettingVarName      : "",
                    SettingDesc         : "",
                    SettingVarType      : "",
                    SettingDefault      : "",
                    SettingWinDefault   : "",
                    SettingLnxDefault   : "",
                    SettingScope        : "",
                    SettingFriendlyName : "",
                    SettingStringLength : "",
                    SettingRegistryType : "",
                    SettingWhitelist    : "",
                    SettingBlacklist    : "",
                    SettingHash         : "",
                    SettingDevDriver    : "",
                    SettingMinVersion   : "",
                    SettingMaxVersion   : "",}
    while True:
        if line == "":
            errorExit("End of file found when processing a Leaf: %s:%d" %
                      (fileReader.getFilename(), fileReader.getLineNumber()))
        elif line.find("}") >= 0:
            # end of the leaf found, let's get out of here!
            break
        for key, value in SettingParseStrings.items():
            if value in line:
                settingData[key] = extractMultiLineQuotedText(line, fileReader)

        # more stuff to parse, on to the next line
        line = fileReader.readLine()

    if settingData[SettingType] != "":
        settingData[SettingRegistryType] = RegistryTypeTable[settingData[SettingType]]
    if settingData[SettingScope] == "":
        settingData[SettingScope] = SettingScopeDefault
    if settingData[SettingName] != "" and settingData[SettingScope] != "PublicCatalystKey":
        if hashAlgorithm == 1:
            # SHA1 encryption
            settingData[SettingHash] = sha1(settingData[SettingName])
        elif hashAlgorithm == 2:
            # plain text
            settingData[SettingHash] = settingData[SettingName]
        else:
            # default scrambling, for hashAlgorithm == 0 and other cases not handled above.
            settingData[SettingHash] = "#" + str(fnv1a(settingData[SettingName]))
    return settingData

def parseConfigFile(configFileName):
    return parseConfigFile2(configFileName, 0)

def parseConfigFile2(configFileName, hashAlgorithm):
    fileReader            = FileReader(configFileName)
    enumDefs              = []
    settingDefs           = []
    nodeList              = []
    nodeParentIndexList   = []
    hwl                   = ""
    inLeaf                = False
    parentIndexStack      = [-1] # Always keep -1 on the stack since it represents "No Parent"
    while True:
        line = fileReader.readLine()
        if line == "":
            # we've reached the end of the file, break out of the while loop
            break
        elif isCfgComment(line):
            # nothing to do for comments, just continue to the next line
            continue

        # Enum definition
        if line.find(StartEnum) >= 0 or line.find(StartPrivEnum) >= 0:
            enumDefs.append(parseEnum(line, fileReader))

        # Start Node
        if line.find(StartNode) >= 0:
            # Use the index from the top of the parent index stack as the parent index for the current node
            nodeParentIndexList.append(parentIndexStack[-1])

            # Add the current node to the node list
            nodeList.append(extractDoubleQuotedText(line))

            # Add the index of the current node to the top of the parent index stack
            parentIndexStack.append(len(nodeList) - 1)

        if line.find(StartLeaf) >= 0:
            line = fileReader.readLine()
            if line.find("{") == -1:
                errorExit("Malformed Leaf, missing start bracket: %s:%d" %
                          (fileReader.getFilename(), fileReader.getLineNumber()))
            settingDefs.append(parseLeaf(line, fileReader, hashAlgorithm))
            settingDefs[-1][SettingNode] = nodeList[-1]

        if line.find(StartHwl) >= 0:
            hwl = extractDoubleQuotedText(line)

        # This field is only used internal to the configParser. It is optionally defined at the top of the
        # config file and specifies the scope to use when a setting defintion is missing the SettingScope field
        global SettingScopeDefault
        if line.find(StartDefaultScope) >= 0:
            SettingScopeDefault = extractDoubleQuotedText(line)

        if line.strip() == "}":
            # The parent index stack should have at least 2 indices in it if we're inside a node
            if len(parentIndexStack) > 1:
                # Pop the top of the parent index stack off since we've found the node's closing bracket
                parentIndexStack.pop()
                if inLeaf:
                    errorExit("inLeaf when we encountered an end of node close bracket: %s:%d" %
                              (fileReader.getFilename(), fileReader.getLineNumber()))
                    inLeaf = False
            else:
                errorExit("Unexpected end bracket: %s:%d" %
                          (fileReader.getFilename(), fileReader.getLineNumber()))
    fileReader.close()
    return {Enums: enumDefs, Settings: settingDefs, Nodes: nodeList, Hwl: hwl, NodeParentIndices: nodeParentIndexList}

