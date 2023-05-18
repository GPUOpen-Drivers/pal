##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

include_guard()

# All options/cache variables should have the prefix "PAL_" this serves two main purposes
# - Name collision issues
# - cmake-gui allows grouping of variables based on prefixes, which then makes it clear what options PAL defined

option(PAL_ENABLE_PRINTS_ASSERTS "Enable print assertions?")
option(PAL_ENABLE_PRINTS_ASSERTS_DEBUG "Enable print assertions on debug builds?" ON)

option(PAL_ENABLE_LOGGING "Enable debug logging?" ON)

option(PAL_MEMTRACK "Enable PAL memory tracker?")

option(PAL_64BIT_ARCHIVE_FILE_FMT "DXCP requires 64-bit file archives to allow creation of files >4GB. Vulkan requires 32-bit file archives for backwards compatibility. Clients may choose your preference here. 32-bit by default." OFF)
