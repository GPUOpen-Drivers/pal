##
 #######################################################################################################################
 #
 #  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
from functools import lru_cache

def find_upwards(needles):
    drive, curdir = os.path.splitdrive(os.path.dirname(__file__))
    while True:
        for needle in needles:
            fullpath = os.path.join(drive, curdir, needle)
            if os.path.isfile(fullpath):
                return os.path.join(drive, curdir)
        if curdir == os.path.sep:
            return None
        curdir = os.path.dirname(curdir)

@lru_cache()
def get_pal_root():
    return find_upwards(["inc/core/pal.h"])

def from_pal_root(*path):
    return os.path.join(get_pal_root(), *path)

