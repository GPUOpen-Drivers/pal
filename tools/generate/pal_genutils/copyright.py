##
 #######################################################################################################################
 #
 #  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

import datetime
from . import paths

CPFH = open(paths.from_pal_root("tools", "pal-copyright-template.txt"), 'r')
fCopyright = CPFH.readline().strip()
CPFH.close()

# Generate a copyright for a given year
def GenCopyrightForYear(minyear=None, maxyear=None):
    if maxyear is None:
        yearRange = str(datetime.date.today().year)
    else:
        yearRange = str(maxyear)
    if minyear is not None:
        yearRange = f"{minyear}-{yearRange}"

    return fCopyright.format(yearRange=yearRange)

# Wrap a copyright in a single or multiline comment block (C/C++)
def ConvertToCppComment(copyright):
    lines = copyright.strip().split("\n")
    if len(lines) == 1:
        return f"/* {lines[0]} */\n"

    outstr = "/*\n"
    outstr += " *" * 119 + "\n"
    for line in lines:
        if line:
            outstr += f" * {line}\n"
        else:
            outstr += " *\n"
    outstr += " " + "*" * 118 + "/\n"
    return outstr

# Wrap a copyright in a single or multiline comment block (Python/CMake/Bash)
def ConvertToHashComment(copyright):
    lines = copyright.strip().split("\n")
    if len(lines) == 1:
        return f"### {lines[0]} ###\n"

    outstr = "#" * 120 + "\n"
    for line in lines:
        if line:
            outstr += f"# {line}\n"
        else:
            outstr += "#\n"
    outstr += "#" * 120 + "/\n"
    return outstr

# Simple default copyright strings
Copyright = GenCopyrightForYear()
CppCopyright = ConvertToCppComment(Copyright)
