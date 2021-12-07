##
 #######################################################################################################################
 #
 #  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

add_subdirectory(zstd)

# This allows clients of RDF to have their own copy of catch2/cli11/json, and
# still build RDF "in-tree" when imported via a submodule. This is just
# convenience, when building in-tree there is no guarantee that tools & tests
# will work
if(RDF_BUILD_TESTS)
	if(NOT TARGET catch2)
		add_subdirectory(catch2)
	endif()
endif()

if(RDF_BUILD_TOOLS)
	if(NOT TARGET cli11)
		add_subdirectory(cli11)
	endif()
	if(NOT TARGET json)
		add_subdirectory(json)
	endif()
endif()
