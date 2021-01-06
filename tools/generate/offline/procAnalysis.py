##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

import re
import os,sys

FileHeaderCopyright = '/*\n\
 ***********************************************************************************************************************\n\
 *\n\
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.\n\
 *\n\
 *  Permission is hereby granted, free of charge, to any person obtaining a copy\n\
 *  of this software and associated documentation files (the "Software"), to deal\n\
 *  in the Software without restriction, including without limitation the rights\n\
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n\
 *  copies of the Software, and to permit persons to whom the Software is\n\
 *  furnished to do so, subject to the following conditions:\n\
 *\n\
 *  The above copyright notice and this permission notice shall be included in all\n\
 *  copies or substantial portions of the Software.\n\
 *\n\
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n\
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n\
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n\
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n\
 *  SOFTWARE.\n\
 *\n\
 **********************************************************************************************************************/\n\
'

class Param:
    def __init__(self, full_str):
        self.full_str = full_str

        self.type_str = None
        self.identifier = None

        self.is_variadic = False
        self.is_array = False
        self.is_function_pointer = False
        self.is_pointer = False

    def GetParameterStr(self, param_format_size):
        if self.is_variadic:
            return self.full_str

        if self.is_array or self.is_function_pointer:
            first_half, second_half = self.full_str.split(' ', 1)
        else:
            first_half = self.type_str
            second_half = self.identifier

        return "%s %*.s %s" % (
                first_half,
                param_format_size - len(first_half),
                ' ',
                second_half)

    def GetType(self):
        return self.type_str

    def GetValue(self):
        return self.identifier

def GetFmtType(ftype):
    if ftype.find('*') != -1:
        return "%p"
    elif ftype.find('Ptr') != -1:
        return "%p"
    elif ftype.find('_cookie_t') != -1:
        return "%p"
    elif ftype.find('int64') != -1:
        return "%lx"
    elif ftype.find('int') != -1:
        return "%x"
    elif ftype.find('sem_handle') != -1:
        return "%x"
    elif ftype.find('handle_type') != -1:
        return "%x"
    elif ftype.find('_handle') != -1:
        return "%p"
    elif ftype.find('signed') != -1:
        return "%x"
    else:
        return "%x"

def SplitTypeAndId(param_str):
    sep_index = max(param_str.rfind(' '), param_str.rfind('*'))
    if param_str[sep_index] == '*':
        type_str = param_str[:sep_index + 1]
    else:
        type_str = param_str[:sep_index]
    identifier = param_str[sep_index + 1:]
    return type_str, identifier

class EntryPoint:
    def __init__(
    self,
    line,
    ):
        funcConst = re.compile('^.+(const [\w_]+ ?\*?) +([\w_]+) +\((.*)\)' )
        func = re.compile('^.+ ([\w_]+ ?\*?) +([\w_]+ ?) +\((.*)\)' )
        if funcConst.search(line):
            [function] = funcConst.findall(line)
        else:
            [function] = func.findall(line)
        # function[0] is the return type
        # function[1] is the function name
        # function[2] is the param type and value or simply 'void'.
        self.ret = function[0].strip(' ')
        self.api = function[1].strip(' ')
        self.name = ''
        # record name that matches coding standard.
        if self.api.find('_'):
            apis = self.api.split('_')
            for api in apis:
                self.name += api[0].upper() + api[1:]

        self.params = []
        params = function[2].split(',')
        for p in params:
            p = p.strip(' ')
            if p == '' or p == 'void':
                continue

            pm = Param(p)
            if p == '...':
                # variadic argument
                pm.type_str = '...'
                pm.is_variadic = True

            elif p.endswith(']'):
                # array type, e.g. int a[], int a[10]
                bracket_sep_index = p.rfind('[')
                type_suffix = p[bracket_sep_index:]
                type_prefix, id_str = SplitTypeAndId(p[:bracket_sep_index])
                pm.type_str = type_prefix + type_suffix
                pm.identifier = id_str
                pm.is_array = True

            elif p.endswith(')'):
                # function pointer type, e.g. void (*foo)(int)
                parentheses_sep_indx = p.rfind(')', 0, -1)
                type_suffix = p[parentheses_sep_indx:]
                type_prefix, id_str = SplitTypeAndId(p[:parentheses_sep_indx])
                pm.type_str = type_prefix + type_suffix
                pm.identifier = id_str
                pm.is_function_pointer = True

            else:
                pm.type_str, pm.identifier = SplitTypeAndId(p)
                pm.is_pointer = pm.type_str.endswith('*')

            self.params.append(pm)

    def GetFormattedName(self):
        return self.name
    def GetFunctionName(self):
        return self.api
    def GetFunctionRetType(self):
        return self.ret
    def GetFunctionParams(self):
        return self.params
class Variable:
    def __init__(self, line):
        line.rstrip()
        variable = re.compile('^.+ ([\w_]+ ?\*?) +([\w_]+ ?)')
        self.library = line.split(' ')[0].strip(' ')
        [values] = variable.findall(line)
        self.varName = values[1].strip(' ')
        self.varType = values[0].strip(' ')
        self.name = ''
        # record name that matches coding standard.
        if self.varName.find('_'):
            names = self.varName.split('_')
            for name in names:
                self.name += name[0].upper() + name[1:]
    def GetFormattedName(self):
        return self.name
    def GetVarType(self):
        return self.varType
    def GetVarName(self):
        return self.varName
    def GetLibrary(self):
        return self.library
class ProcMgr:
    def __init__(self, fileName, libraryDict, needSpecializedInit):
        self.fileName = fileName
        self.libraryDict = libraryDict
        self.libraries = {}
        self.needSpecializedInit = needSpecializedInit
        self.var = []
        fp  = open(fileName)
        contents = fp.readlines()
        for i, content in enumerate(contents):
            content.rstrip()
            if content.find('#') != -1:
                pass
            else:
                # get the library name where we can dlsym the symbol
                if content.find('@var') != -1:
                    var = Variable(content)
                    self.var.append(var)
                else:
                    library = content.split(' ')[0]
                    ep = EntryPoint(
                    content,
                    )
                    self.add(ep, library)
    def add(self, entry, library):
        if self.libraries.has_key(library):
            self.libraries[library].append(entry)
        else:
            self.libraries[library] = [entry]
    def GenerateIntro(self, fp, intro):
        fp.write("///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n")
        fp.write("//\n")
        fp.write("// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!\n")
        fp.write("//\n")
        fp.write("// This code has been generated automatically. Do not hand-modify this code.\n")
        fp.write("//\n")
        fp.write("// " + intro + "\n")
        fp.write("//\n")
        fp.write("// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!\n")
        fp.write("///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n\n")
    def GenerateHeader(self, fp):
        fp.write(FileHeaderCopyright)
    def GenerateFunctionDeclaration(self, fp, name):
        for key in self.libraries.keys():
            fp.write("// symbols from " + key + "\n")
            for entry in self.libraries[key]:
                fp.write( "typedef " + entry.GetFunctionRetType() + " (*" + entry.GetFormattedName() + ")(")
                params = entry.GetFunctionParams()
                if len(params) == 0:
                    fp.write("void);\n\n")
                else:
                    index = 0
                    length = 0
                    for pa in params:
                        if length < len(pa.GetType()):
                            length = len(pa.GetType())
                    if length % 4 != 0:
                        length = (length + 4) - length % 4

                    for pa in params:
                        if index == 0:
                            index = 1
                        else:
                            fp.write(",")
                        fp.write("\n            %s" % pa.GetParameterStr(length))
                    fp.write(");\n")
                    fp.write("\n")
    def GetFormattedLibraryName(self, name):
        ret = ''
        if self.libraryDict.has_key(name):
            ret = self.libraryDict[name]
        else:
            if name.find('^lib') != -1:
                name.replace('^lib','')
                ret = 'Lib'
            if name.find('-') != -1:
                library = name.split('-')
                for libs in library:
                    ret += libs[0].upper() + libs[1:]
            else:
                ret += name[0].upper() + name[1:]
            ret = ret.split('.')[0]
        return ret

    def GenerateLibrariesEnum(self, fp, name):
        # add libraries's enum
        fp.write("enum " + name + "Libraries : uint32\n")
        fp.write("{\n")
        i = 0
        for key in self.libraries.keys():
            formattedName = self.GetFormattedLibraryName(key)
            fp.write("    " + formattedName + " = " + str(i) + ",\n")
            i += 1
        fp.write("    " + name + "LibrariesCount = " + str(i) + "\n")
        fp.write("};\n\n")

    def GenerateClassDeclaration(self, fp, name):
        fp.write("class Platform;\n\n")
        self.GenerateCommentLine(fp)
        fp.write("// the class is responsible for resolving all external symbols that required by the Dri3WindowSystem.")
        fp.write("\nclass " + name + "\n")
        fp.write("{\n")

    def GenerateMemberToClass(self, fp, name):
        # add class definition
        fp.write("public:\n")
        fp.write("    " + name + "();\n")
        fp.write("    ~" + name + "();\n\n")
        fp.write("    bool   Initialized() { return m_initialized; }\n\n")
        fp.write("    const " + name + "Funcs& GetProcsTable()const { return m_funcs; }\n")
        fp.write("#if defined(PAL_DEBUG_PRINTS)\n")
        fp.write("    const " + name + "FuncsProxy& GetProcsTableProxy()const { return m_proxy; }\n")
        fp.write("\n    void SetLogPath(const char* pPath) { m_proxy.Init(pPath); }\n")
        fp.write("#endif\n")
        fp.write("\n    Result Init(Platform* pPlatform);\n")
        if self.needSpecializedInit:
            fp.write("    void   SpecializedInit(Platform* pPlatform, char*  pDtifLibName);\n")
        if self.var:
            fp.write("\n")
            for var in self.var:
                fp.write("    " + var.GetVarType() + "* Get" + var.GetFormattedName() + "() const;\n")
        # add library handler
        fp.write("\nprivate:\n")
        if self.var:
            for var in self.var:
                fp.write("    " + var.GetVarType() + "* m_p" + var.GetFormattedName() +";\n")
            fp.write("\n")
        fp.write("    Util::Library m_library[" + name + "LibrariesCount];\n")
        fp.write("    bool          m_initialized;\n\n")
        fp.write("    "  + name + "Funcs      m_funcs;\n")
        fp.write("#if defined(PAL_DEBUG_PRINTS)\n")
        fp.write("    "  + name + "FuncsProxy m_proxy;\n")
        fp.write("#endif\n\n")
        fp.write("    PAL_DISALLOW_COPY_AND_ASSIGN(" + name + ");\n");

    def GenerateStubFunctions(self, fp, name):
        # Adding constructor for proxy
        self.GenerateCommentLine(fp)
        fp.write("#if defined(PAL_DEBUG_PRINTS)\n")
        fp.write("void " + name + "FuncsProxy::Init(const char* pLogPath)\n")
        fp.write("{\n")
        fp.write("    char file[128] = {0};\n")
        fp.write("    Util::Snprintf(file, sizeof(file), \"%s/" + name + "TimeLogger.csv\", pLogPath);\n")
        fp.write("    m_timeLogger.Open(file, FileAccessMode::FileAccessWrite);\n")
        fp.write("    Util::Snprintf(file, sizeof(file), \"%s/" + name + "ParamLogger.trace\", pLogPath);\n")
        fp.write("    m_paramLogger.Open(file, FileAccessMode::FileAccessWrite);\n")
        fp.write("}\n\n")
        # adding stub function for each function pointer
        for key in self.libraries.keys():
            for entry in self.libraries[key]:
                self.GenerateCommentLine(fp)
                fp.write(entry.GetFunctionRetType() + " " +name + "FuncsProxy::pfn" + entry.GetFormattedName() + "(")
                params = entry.GetFunctionParams()
                param = ''
                if len(params) == 0:
                    fp.write("    ) const\n")
                else:
                    length = 0
                    for pa in params:
                        if length < len(pa.GetType()):
                            length = len(pa.GetType())
                    dot = ""
                    for pa in params:
                        fp.write("%s\n    %s"  %(dot, pa.GetParameterStr(length)))
                        dot = ','
                    fp.write("\n    ) const\n")
                fp.write("{\n")
                fp.write("    const int64 begin = Util::GetPerfCpuTime();\n")
                paramIndent = 0

                if len(params) > 0 and params[-1].is_variadic:
                    fp.write("    va_list args;\n")
                    fp.write("    va_start(args, %s);\n" % params[-2].GetValue())

                if entry.GetFunctionRetType() != "void":
                    if entry.GetFunctionRetType().find('*') != -1:
                        fp.write("    " + entry.GetFunctionRetType() + " pRet = ")
                        paramIndent += 4 + len(entry.GetFunctionRetType()) + 8
                    else:
                        fp.write("    " + entry.GetFunctionRetType() + " ret = ")
                        paramIndent += 4 + len(entry.GetFunctionRetType()) + 7
                    fp.write("m_pFuncs->pfn")
                    paramIndent += 13
                else:
                    fp.write("    m_pFuncs->pfn")
                    paramIndent += 17
                fp.write(entry.GetFormattedName() + "(")
                paramIndent += len(entry.GetFormattedName()) + 1
                indent = ''
                argument = ''
                strfmt = ''
                for pa in params:
                    if pa.is_variadic:
                        argument += indent + "args" + ",\n"
                    else:
                        if '[' in pa.GetValue():
                            pa.value = pa.GetValue()[:pa.GetValue().index('[')]
                        argument += indent + pa.GetValue() + ",\n"
                        indent = ' ' * paramIndent
                        strfmt += GetFmtType(pa.GetType()) + ', '
                if len(params) == 0:
                    fp.write(");\n")
                    strfmt = ', '
                else:
                    fp.write("%s);\n" %(argument[:-2]))

                if len(params) > 0 and params[-1].is_variadic:
                    fp.write("    va_end(args);\n")

                fp.write("    const int64 end = Util::GetPerfCpuTime();\n")
                fp.write("    const int64 elapse = end - begin;\n")
                fp.write("    m_timeLogger.Printf(\"" + entry.GetFormattedName() + ",%ld,%ld,%ld\\n\", begin, end, elapse);\n")
                fp.write("    m_timeLogger.Flush();\n\n")

                if len(params) == 0:
                    fp.write("    m_paramLogger.Printf(\"" + entry.GetFormattedName() + "()\\n\");\n")
                else:
                    logString = "    m_paramLogger.Printf(\n        \"" + entry.GetFormattedName() + "("
                    logIndentLen = 8
                    logParam = ''
                    indent = ' ' * logIndentLen
                    for pa in params:
                        if pa.is_variadic:
                            continue
                        if pa.GetType().find('_cookie_t') != -1:
                            logParam += indent + "&" + pa.GetValue() + ",\n"
                        else:
                            logParam += indent + pa.GetValue() + ",\n"
                    fp.write(logString + strfmt[:-2] + ")\\n\",\n" + logParam[:-2] + ");\n")
                fp.write("    m_paramLogger.Flush();\n")
                if entry.GetFunctionRetType() != "void":
                    if entry.GetFunctionRetType().find('*') != -1:
                        fp.write("\n    return pRet;\n")
                    else:
                        fp.write("\n    return ret;\n")
                fp.write("}\n")
                fp.write("\n")
        fp.write("#endif\n\n")
    def GenerateCommentLine(self, fp):
        fp.write("// =====================================================================================================================\n")
    def GenerateConstructor(self, fp, name):
        # initialize static variables
        self.GenerateCommentLine(fp);
        fp.write(name + "::" + name + "()\n")
        fp.write("    :\n")
        for var in self.var:
            fp.write("    " + "m_p" + var.GetFormattedName() + "(nullptr),\n")

        fp.write("    m_initialized(false)\n")
        fp.write("{\n")
        fp.write("    " + "memset(&m_funcs, 0, sizeof(m_funcs));\n")
        fp.write("}\n\n")

        for var in self.var:
            self.GenerateCommentLine(fp);
            fp.write("" + var.GetVarType() + "* " + name + "::Get" + var.GetFormattedName() + "() const\n")
            fp.write("{\n")
            fp.write("    return m_p" + var.GetFormattedName() + ";\n")
            fp.write("}\n\n")

    def GenerateInitFunction(self, fp, name):
        # implement Init function
        self.GenerateCommentLine(fp)
        fp.write("Result " + name + "::Init(\n")
        fp.write("    Platform* pPlatform)\n")
        fp.write("{\n")
        fp.write("    Result           result      = Result::Success;\n")
        fp.write("    constexpr uint32 LibNameSize = 64;\n")
        # On Android, there is no libdrm.so.2 and libdrm_amdgpu.so.1. There are only libdrm.so and libamdgpu_drm.so.
        # Generate the code for Android accordingly.

        androidLibdrm = [];
        for key in self.libraries.keys():
            if key.find("libdrm") >= 0:
                # Get the base name and append ".so" to generate the library name without version.
                androidLibdrm.append(key.split('.')[0] + '.so');
                print key.split('.')[0]

        if len(androidLibdrm) > 0:
            fp.write("#if PAL_BUILD_ANDROID\n")
            fp.write("    char LibNames[" + name + "LibrariesCount][LibNameSize] = {\n")
            for key in androidLibdrm:
                fp.write("        \"" + key + "\",\n")
            fp.write("    };\n")
            fp.write("#else\n")

        fp.write("    char LibNames[" + name + "LibrariesCount][LibNameSize] = {\n")
        for key in self.libraries.keys():
            fp.write("        \"" + key + "\",\n")
        fp.write("    };\n")

        if len(androidLibdrm) > 0:
            fp.write("#endif\n")

        if (self.needSpecializedInit):
            fp.write("    SpecializedInit(pPlatform, &LibNames[LibDrmAmdgpu][0]);\n")
        # load function point from libraries.
        fp.write("    if (m_initialized == false)\n")
        fp.write("    {\n")
        for key in self.libraries.keys():
            libraryEnum = self.GetFormattedLibraryName(key)
            fp.write("        // resolve symbols from " + key + "\n")
            fp.write("        result = m_library[" + libraryEnum + "].Load(LibNames[" + libraryEnum + "]);\n")
            fp.write("        PAL_ASSERT_MSG(result == Result::Success, \"Failed to load " + self.libraryDict[key] +" library\");\n")
            fp.write("        if (result == Result::Success)\n")
            fp.write("        {\n")
            for entry in self.libraries[key]:
                fp.write("            m_library[" + libraryEnum + "].GetFunction(\"" + entry.GetFunctionName() + "\", &m_funcs.pfn" + entry.GetFormattedName() + ");\n")
            fp.write("        }\n\n")
        for var in self.var:
            libraryEnum = self.GetFormattedLibraryName(var.GetLibrary())
            fp.write("        if (m_library[" + libraryEnum + "].IsLoaded() == false)\n")
            fp.write("        {\n")
            fp.write("            result = Result::ErrorUnavailable;\n")
            fp.write("        }\n")
            fp.write("        else\n")
            fp.write("        {\n")
            fp.write("            m_library[" + libraryEnum + "].GetFunction(\"" + var.GetVarName() + "\", &m_p" + var.GetFormattedName() + ");\n")
            fp.write("        }\n")
        fp.write("        if (result == Result::Success)\n")
        fp.write("        {\n")
        fp.write("            m_initialized = true;\n")
        fp.write("#if defined(PAL_DEBUG_PRINTS)\n")
        fp.write("            m_proxy.SetFuncCalls(&m_funcs);\n");
        fp.write("#endif\n")
        fp.write("        }\n")
        fp.write("    }\n")
        fp.write("    return result;\n")
        fp.write("}\n\n")

    def GenerateFuncStruct(self, fp, name):
        fp.write("struct " + name + "Funcs\n")
        fp.write("{\n")
        length = 0

        # find the max length of return value.
        for key in self.libraries.keys():
            for entry in self.libraries[key]:
                if length < len(entry.GetFormattedName()):
                    length = len(entry.GetFormattedName())
        if length % 4 != 0:
            length = length + 4 - length % 4

        for key in self.libraries.keys():
            for entry in self.libraries[key]:
                fp.write("    %s %*.s pfn%s;\n" %(entry.GetFormattedName(), length - len(entry.GetFormattedName()), " ", entry.GetFormattedName()))
                fp.write("    bool pfn" + entry.GetFormattedName() + "isValid() const\n")
                fp.write("    {\n")
                fp.write("        return (pfn" + entry.GetFormattedName() + " != nullptr);\n")
                fp.write("    }\n")
                fp.write("\n")
        fp.write("};\n\n")
    def GenerateFuncProxy(self, fp, name):
        self.GenerateCommentLine(fp)
        fp.write("// the class serves as a proxy layer to add more functionality to wrapped callbacks.\n")
        fp.write("#if defined(PAL_DEBUG_PRINTS)\n")
        fp.write("class " + name + "FuncsProxy\n")
        fp.write("{\n")
        fp.write("public:\n")
        fp.write("    " + name + "FuncsProxy() { }\n")
        fp.write("    ~" + name + "FuncsProxy() { }\n\n")
        fp.write("    void SetFuncCalls(" + name + "Funcs* pFuncs) { m_pFuncs = pFuncs; }\n\n")
        fp.write("    void Init(const char* pPath);\n\n")
        for key in self.libraries.keys():
            for entry in self.libraries[key]:
                fp.write( "    " + entry.GetFunctionRetType() + " pfn" + entry.GetFormattedName() + "(")
                params = entry.GetFunctionParams()
                if len(params) == 0:
                    fp.write("void) const;\n\n")
                else:
                    index = 0
                    length = 0
                    for pa in params:
                        if length < len(pa.GetType()):
                            length = len(pa.GetType())
                    if length % 4 != 0:
                        length = (length + 4) - length % 4

                    for pa in params:
                        if index == 0:
                            index = 1
                        else:
                            fp.write(",")
                        fp.write("\n            %s" % pa.GetParameterStr(length))
                    fp.write(") const;\n\n")
                    fp.write("    bool pfn" + entry.GetFormattedName() + "isValid() const\n")
                    fp.write("    {\n")
                    fp.write("        return (m_pFuncs->pfn" + entry.GetFormattedName() + " != nullptr);\n")
                    fp.write("    }\n")
                    fp.write("\n")
        fp.write("private:\n")
        fp.write("    Util::File  m_timeLogger;\n")
        fp.write("    Util::File  m_paramLogger;\n")
        fp.write("    " + name + "Funcs* m_pFuncs;\n\n")
        fp.write("    PAL_DISALLOW_COPY_AND_ASSIGN(" + name + "FuncsProxy);\n");
        fp.write("};\n")
        fp.write("#endif\n\n")
    def GenerateDestructor(self, fp, name):
        # initialize static variables
        self.GenerateCommentLine(fp);
        fp.write(name + "::~" + name + "()\n{\n}\n\n")

    def GenerateHeadFile(self, fp, name):
        self.GenerateFunctionDeclaration(fp, name)
        self.GenerateLibrariesEnum(fp, name)
        self.GenerateFuncStruct(fp, name)
        self.GenerateFuncProxy(fp, name)
        self.GenerateClassDeclaration(fp, name)
        self.GenerateMemberToClass(fp, name)
        fp.write("};\n\n")

    def GenerateCppFile(self, fp, name):
        self.GenerateStubFunctions(fp, name)
        self.GenerateConstructor(fp, name)
        self.GenerateDestructor(fp, name)
        self.GenerateInitFunction(fp, name)

    def printEntryList(self):
        for key in self.libraries.keys():
            print "in library %s" %(key)
            for entry in self.libraries[key]:
                params = entry.GetFunctionParams()
                if len(params) == 0:
                    param = 'void '
                else:
                    param = ''
                    for pa in params:
                        param += pa.GetType() + ' ' + pa.GetValue()
                        param += ','
                print "ret = %s, func = %s, param = %s" %(entry.GetFunctionRetType(), entry.GetFunctionName(), param[:-1])
