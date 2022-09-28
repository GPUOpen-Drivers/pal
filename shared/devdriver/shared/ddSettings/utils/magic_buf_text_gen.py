##
 #######################################################################################################################
 #
 #  Copyright (c) 2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

import argparse
import sys

def main():
    parser = argparse.ArgumentParser(
        description="Generate magic buffer in text format to be included in C++ files."
    )

    parser.add_argument(
        "magic_file", type=str, help="Path to the magic-buffer binary file."
    )
    parser.add_argument("output", type=str, help="Path to the output file.")

    args = parser.parse_args()

    with open(args.magic_file, "rb") as file:
        magic_buf = file.read()

    magic_text = ", ".join(map(str, magic_buf))

    with open(args.output, "w") as file:
        file.write(magic_text)

if __name__ == "__main__":
    sys.exit(main())
