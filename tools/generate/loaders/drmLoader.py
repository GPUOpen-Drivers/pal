##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

def main(fileName, outputDir="./"):
    # The dict to maintain the mapping between library name and generated enum name.
    # It is better to keep in caller script.
    libraryDict = {
        "libdrm_amdgpu.so.1" : "LibDrmAmdgpu",
        "libdrm.so.2"    : "LibDrm",
    }

    libraryDictAlts = None

    procMgr  = proc.ProcMgr(fileName,
                            libraryDict,
                            needSpecializedInit=True,
                            libraryDictAlts=libraryDictAlts,
                            needsLibraryGetter=True)

    intro = "Modify the procAnalysis.py and drmLoader.py in the tools/generate directory OR drmLoader.proc instead"

    # let procMgr generate the class named as Dri3Loader
    fp = open(os.path.join(outputDir, "g_drmLoader.h"),'w')
    procMgr.GenerateHeader(fp)
    procMgr.GenerateIntro(fp, intro)
    # adding special include files or customized lines
    fp.write("#pragma once\n\n")
    fp.write("#include \"core/os/amdgpu/amdgpuHeaders.h\"\n")
    fp.write("#include \"palFile.h\"\n")
    fp.write("#include \"palLibrary.h\"\n\n")
    fp.write("namespace Pal\n")
    fp.write("{\n")
    fp.write("namespace Amdgpu\n")
    fp.write("{\n")
    procMgr.GenerateHeadFile(fp, "DrmLoader")
    fp.write("} // Amdgpu\n")
    fp.write("} // Pal\n")
    fp.close()

    fp = open(os.path.join(outputDir, "g_drmLoader.cpp"), 'w')
    procMgr.GenerateHeader(fp)
    procMgr.GenerateIntro(fp, intro)
    fp.write("#include \"core/os/amdgpu/amdgpuPlatform.h\"\n")
    fp.write("#include \"core/os/amdgpu/g_drmLoader.h\"\n")
    fp.write("#include \"palAssert.h\"\n")
    fp.write("#include \"palSysUtil.h\"\n\n")
    fp.write("#include <dlfcn.h>\n")
    fp.write("#include <time.h>\n")
    fp.write("#include <string.h>\n\n")
    fp.write("using namespace Util;\n\n")
    fp.write("namespace Pal\n")
    fp.write("{\n")
    fp.write("namespace Amdgpu\n")
    fp.write("{\n")
    procMgr.GenerateCppFile(fp, "DrmLoader")
    fp.write("void DrmLoader::SpecializedInit(\n    Platform* pPlatform,\n    char*     pDtifLibName)\n")
    fp.write("{\n")
    fp.write("}\n\n")
    fp.write("} // Linux\n")
    fp.write("} // Pal\n")
    fp.close()

if __name__ == '__main__':
    main(sys.argv[1])
