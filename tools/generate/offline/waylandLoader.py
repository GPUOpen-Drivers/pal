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
    libraryDict = {"libwayland-client.so.0" : "LibWaylandClient"}

    procMgr  = proc.ProcMgr(fileName, libraryDict, 0)
    intro = "Modify the procAnalysis.py and waylandLoader.py in the tools/generate directory OR waylandWindowSystem.proc instead"

    # let procMgr generate the class named as WaylandLoader
    fp = open(os.path.join(outputDir, "g_waylandLoader.h"),'w')
    procMgr.GenerateHeader(fp)
    procMgr.GenerateIntro(fp, intro)
    # adding special include files or customized lines
    fp.write("#pragma once\n\n")
    fp.write("#include \"core/os/amdgpu/wayland/mesa/wayland-dmabuf-client-protocol.h\"\n\n")
    fp.write("#include \"core/os/amdgpu/wayland/mesa/wayland-drm-client-protocol.h\"\n\n")
    fp.write("#ifdef None\n")
    fp.write("#undef None\n")
    fp.write("#endif\n")
    fp.write("#ifdef Success\n")
    fp.write("#undef Success\n")
    fp.write("#endif\n")
    fp.write("#ifdef Always\n")
    fp.write("#undef Always\n")
    fp.write("#endif\n\n")
    fp.write("#include \"palFile.h\"\n")
    fp.write("#include \"palLibrary.h\"\n\n")
    fp.write("using namespace Util;\n\n")
    fp.write("namespace Pal\n")
    fp.write("{\n")
    fp.write("namespace Amdgpu\n")
    fp.write("{\n")
    procMgr.GenerateHeadFile(fp, "WaylandLoader")
    fp.write("} // Amdgpu\n")
    fp.write("} // Pal\n")
    fp.close()

    fp = open(os.path.join(outputDir, "g_waylandLoader.cpp"), 'w')
    procMgr.GenerateHeader(fp)
    procMgr.GenerateIntro(fp, intro)
    fp.write("#include \"core/os/amdgpu/wayland/g_waylandLoader.h\"\n")
    fp.write("#include \"palAssert.h\"\n")
    fp.write("#include \"palSysUtil.h\"\n\n")
    fp.write("#include <string.h>\n\n")
    fp.write("using namespace Util;\n\n")
    fp.write("namespace Pal\n")
    fp.write("{\n")
    fp.write("namespace Amdgpu\n")
    fp.write("{\n")
    procMgr.GenerateCppFile(fp, "WaylandLoader")
    fp.write("} // Amdgpu\n")
    fp.write("} // Pal\n")
    fp.close()

if __name__ == '__main__':
    main(sys.argv[1])
