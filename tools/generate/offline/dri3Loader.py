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
    libraryDict = {"libxcb-sync.so.1"    : "LibXcbSync",
        "libxcb-present.so.0" : "LibXcbPresent",
        "libxcb-dri3.so.0"    : "LibXcbDri3",
        "libxcb-dri2.so.0"    : "LibXcbDri2",
        "libxcb.so.1"         : "LibXcb",
        "libxshmfence.so.1"   : "LibXshmFence",
        "libX11-xcb.so"       : "LibX11Xcb",
        "libX11.so"           : "LibX11"}

    procMgr  = proc.ProcMgr(fileName, libraryDict)
    intro = "Modify the procsAnalysis.py and dri3Loader.py in the tools/generate directory OR dri3WindowSystem.proc instead"

    # let procMgr generate the class named as Dri3Loader
    fp = open("dri3Loader.h",'w')
    procMgr.GenerateHeader(fp)
    procMgr.GenerateIntro(fp, intro)
    # adding special include files or customized lines
    fp.write("#pragma once\n\n")
    fp.write("#include <X11/Xlib.h>\n")
    fp.write("#include <X11/Xutil.h>\n")
    fp.write("#include <X11/extensions/dri2tokens.h>\n")
    fp.write("#ifdef None\n")
    fp.write("#undef None\n")
    fp.write("#endif\n")
    fp.write("#ifdef Success\n")
    fp.write("#undef Success\n")
    fp.write("#endif\n")
    fp.write("#ifdef Always\n")
    fp.write("#undef Always\n")
    fp.write("#endif\n")
    fp.write("#include <xcb/dri3.h>\n")
    fp.write("#include <xcb/dri2.h>\n")
    fp.write("#include <xcb/xcb.h>\n")
    fp.write("#include <xcb/present.h>\n")
    fp.write("extern \"C\"\n")
    fp.write("{\n")
    fp.write("    #include <X11/xshmfence.h>\n")
    fp.write("}\n\n")
    fp.write("#include \"pal.h\"\n")
    fp.write("#include \"palFile.h\"\n")
    fp.write("using namespace Util;\n")
    fp.write("namespace Pal\n")
    fp.write("{\n")
    fp.write("namespace Linux\n")
    fp.write("{\n")
    procMgr.GenerateHeadFile(fp, "Dri3Loader")
    fp.write("} //namespace Linux\n")
    fp.write("} //namespace Pal\n")
    fp.close()

    fp = open("dri3Loader.cpp", 'w')
    procMgr.GenerateHeader(fp)
    procMgr.GenerateIntro(fp, intro)
    fp.write("#include <dlfcn.h>\n")
    fp.write("#include <string.h>\n")
    fp.write("#include <xcb/xcb.h>\n")
    fp.write("#include \"core/os/lnx/dri3/dri3Loader.h\"\n")
    fp.write("#include \"palAssert.h\"\n\n")
    fp.write("#include \"palSysUtil.h\"\n\n")
    fp.write("using namespace Util;\n")
    fp.write("namespace Pal\n")
    fp.write("{\n")
    fp.write("namespace Linux\n")
    fp.write("{\n")
    procMgr.GenerateCppFile(fp, "Dri3Loader")
    fp.write("void\nDri3Loader::SpecializedInit(Platform* pPlatform)\n")
    fp.write("{\n}\n\n")
    fp.write("} //namespace Linux\n")
    fp.write("} //namespace Pal\n")
    fp.close()
