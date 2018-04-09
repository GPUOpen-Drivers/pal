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

import procAnalysis as proc
import os,sys

if __name__ == '__main__':
    fileName = sys.argv[1]
    # The dict to maintain the mapping between library name and generated enum name.
    # It is better to keep in caller script.
    libraryDict = {"libdrm.so.2"    : "LibDrm",
                   "libdrm_amdgpu.so.1" : "LibDrmAmdgpu"}

    procMgr  = proc.ProcMgr(fileName, libraryDict)

    intro = "Modify the procsAnalysis.py and drmLoader.py in the tools/generate directory OR drmLoader.proc instead"

    # let procMgr generate the class named as Dri3Loader
    fp = open("drmLoader.h",'w')
    procMgr.GenerateHeader(fp)
    procMgr.GenerateIntro(fp, intro)
    # adding special include files or customized lines
    fp.write("#pragma once\n\n")
    fp.write("#include \"pal.h\"\n")
    fp.write("#include \"core/os/lnx/lnxHeaders.h\"\n")
    fp.write("#include \"palFile.h\"\n")
    fp.write("namespace Pal\n")
    fp.write("{\n")
    fp.write("namespace Linux\n")
    fp.write("{\n")
    procMgr.GenerateHeadFile(fp, "DrmLoader")
    fp.write("} //namespace Linux\n")
    fp.write("} //namespace Pal\n")
    fp.close()

    fp = open("drmLoader.cpp", 'w')
    procMgr.GenerateHeader(fp)
    procMgr.GenerateIntro(fp, intro)
    fp.write("#include <dlfcn.h>\n")
    fp.write("#include <time.h>\n")
    fp.write("#include <string.h>\n")
    fp.write("#include \"core/os/lnx/lnxPlatform.h\"\n")
    fp.write("#include \"core/os/lnx/drmLoader.h\"\n")
    fp.write("#include \"palAssert.h\"\n")
    fp.write("#include \"palSysUtil.h\"\n\n")
    fp.write("using namespace Util;\n")
    fp.write("namespace Pal\n")
    fp.write("{\n")
    fp.write("namespace Linux\n")
    fp.write("{\n")
    fp.write("class Platform;\n")
    procMgr.GenerateCppFile(fp, "DrmLoader")
    fp.write("void\nDrmLoader::SpecializedInit(Platform* pPlatform, char* pDtifLibName)\n")
    fp.write("{\n")
    fp.write("#if PAL_BUILD_DTIF\n")
    fp.write("    if (pPlatform->IsDtifEnabled())\n")
    fp.write("    {\n")
    fp.write("        if (dlopen(\"libtcore2.so\", RTLD_LAZY | RTLD_GLOBAL) != nullptr)\n")
    fp.write("        {\n")
    fp.write("            m_libraryHandles[LibDrmAmdgpu] = dlopen(\"libdtif.so\", RTLD_LAZY | RTLD_GLOBAL);\n")
    fp.write("            if (m_libraryHandles[LibDrmAmdgpu] != nullptr)\n")
    fp.write("            {\n")
    fp.write("                auto* pfnDtifCreate = reinterpret_cast<Dtif::DtifCreateFunc*>(dlsym(\n")
    fp.write("                        m_libraryHandles[LibDrmAmdgpu],\n")
    fp.write("                        \"DtifCreate\"));\n")
    fp.write("                if (pfnDtifCreate != nullptr)\n")
    fp.write("                {\n")
    fp.write("                    if (pfnDtifCreate(\"Vulkan\") != nullptr)\n")
    fp.write("                    {\n")
    fp.write("                        strcpy(pDtifLibName, \"libdtif.so\");\n")
    fp.write("                    }\n")
    fp.write("                }\n")
    fp.write("            }\n")
    fp.write("        }\n")
    fp.write("    }\n")
    fp.write("#endif // PAL_BUILD_DTIF\n")
    fp.write("}\n\n")
    fp.write("} //namespace Linux\n")
    fp.write("} //namespace Pal\n")
    fp.close()
