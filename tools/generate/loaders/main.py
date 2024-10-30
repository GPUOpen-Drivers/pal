##
 #######################################################################################################################
 #
 #  Copyright (c) 2019-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

import sys
import dri3Loader, drmLoader, waylandLoader
import argparse

import os,site; site.addsitedir(os.path.join(os.path.dirname(__file__), "../"))
from pal_genutils.paths import from_pal_root

########################################################################################################################
# Parses the input arguments to the script and returns a structure containing the arguments.
########################################################################################################################
def parseArgs():
    parser = argparse.ArgumentParser()

    parser.add_argument('-dri3ProcFile', type=str, default=from_pal_root("src/core/os/amdgpu/dri3/dri3WindowSystem.proc"),
                        help='Path to DRI3 .proc file.')
    parser.add_argument('-dri3OutputDir', type=str, default=from_pal_root("src/core/os/amdgpu/dri3/"),
                        help='Path to DRI3 output directory.')
    parser.add_argument('-drmProcFile', type=str, default=from_pal_root("src/core/os/amdgpu/drmLoader.proc"),
                        help='Path to DRM .proc file.')
    parser.add_argument('-drmOutputDir', type=str, default=from_pal_root("src/core/os/amdgpu/"),
                        help='Path to DRM output directory.')
    parser.add_argument('-waylandProcFile', type=str, default=from_pal_root("src/core/os/amdgpu/wayland/waylandWindowSystem.proc"),
                        help='Path to Wayland .proc file.')
    parser.add_argument('-waylandOutputDir', type=str, default=from_pal_root("src/core/os/amdgpu/wayland/"),
                        help='Path to Wayland output directory.')

    try:
        args = parser.parse_args()
        return args
    except:
        if "-h" not in sys.argv and "--help" not in sys.argv:
            parser.print_help()
        exit(2)

########################################################################################################################
# Main entrypoint. Autogenerates the various loaders for:
#   - DRI3
#   - DRM
#   - Wayland
########################################################################################################################
def main(args):
    dri3Loader.main(   args.dri3ProcFile,    args.dri3OutputDir)
    drmLoader.main(    args.drmProcFile,     args.drmOutputDir)
    waylandLoader.main(args.waylandProcFile, args.waylandOutputDir)

########################################################################################################################
# If the script is run standalone, will parse the input args and start the main function.
########################################################################################################################
if __name__ == "__main__":
    args = parseArgs()
    main(args)
