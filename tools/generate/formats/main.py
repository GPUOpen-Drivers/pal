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

import argparse
import jinja2
import re
import os
import site
import sys
import time
from ruamel.yaml import YAML
from functools import lru_cache
from shared import structs, utils
from typing import List, Dict, Tuple

SCRIPT_DIR = os.path.dirname(__file__)
DATA_DIR = os.path.join(SCRIPT_DIR, "data")
TEMPLATE_DIR = os.path.join(SCRIPT_DIR, "shared")

site.addsitedir(os.path.join(SCRIPT_DIR, "../"))
from pal_genutils import paths

yaml = YAML()

@lru_cache()
def scrape_enum_values(header_file_path: str) -> dict:
    # Scrape a header file (very dumbly) for "FOO = 123" type lines

    out = {} # str -> Optional[int]. None means multiple entries
    matcher = re.compile(r"^\s*(?:(?:constexpr|unsigned|int|uint32|uint32_t) )*\s*([A-Za-z][A-Za-z0-9_]*)\s*=\s*((?:0x[0-9A-Fa-f]+)|\d+)\b")
    if header_file_path:
        with open(paths.from_pal_root(header_file_path)) as header:
            for line in header:
                m = matcher.match(line)
                if m:
                    valName = m.group(1)
                    value = m.group(2)
                    if valName in out:
                        out[valName] = None
                    else:
                        out[valName] = int(value, base=0)
    return out

def gen_base_headers(yaml_file_path: str, output_file_path: str):
    yaml_data = {}
    with open(yaml_file_path, 'r', encoding='utf8') as input_file:
        yaml_data = yaml.load(input_file)
    assert('formats' in yaml_data)

    ifmts = {}
    for format_name, format_data in yaml_data['formats'].items():
        fmt = structs.IFmt.From_Yaml(format_name, format_data)
        ifmts[format_name] = fmt

    environment = jinja2.Environment(loader=jinja2.FileSystemLoader(TEMPLATE_DIR))
    template    = environment.get_template("template_independent.h.j2")

    context = {
        'FileName': os.path.basename(output_file_path),
        'Year': time.strftime("%Y"),
        'ifmts': ifmts
    }

    os.makedirs(os.path.dirname(output_file_path), exist_ok=True)

    with open(output_file_path, 'w', encoding='utf8') as output:
        output.write(template.render(context))

    return ifmts

def retrieve_hfmt_data(files: List[Tuple[str, str]], ifmts: Dict[str, structs.IFmt]):
    hwl = {}

    for ip, filepath in files:
        with open(f"{DATA_DIR}/{filepath}", 'r', encoding='utf8') as input_file:
            hwl[ip] = yaml.load(input_file)

        # We need to take the YAML and convert it to an HFmt.
        hwl[ip]['hwFormats'] = {
            name: structs.HFmt.From_Yaml(name, fmt) for name, fmt in hwl[ip]['hwFormats'].items()
        }
        # Also load the enum header to automatically get values. We need this to generate some
        # order-dependent tables.
        enumVals = scrape_enum_values(hwl[ip]['config']['enumHeader'])

        # We now need to make separate dictionaries of enum-value (buffer, texture) to PAL format.
        buffer_enums = {}
        texture_enums = {}
        for name, hfmt in hwl[ip]['hwFormats'].items():
            buffer_enum       = hfmt.buffer['enum']
            if buffer_enum not in enumVals:
                raise Exception(f"Buffer enum '{buffer_enum}' for '{name}' in '{ip}' not found in enum header.")
            if enumVals[buffer_enum] is None:
                raise Exception(f"Buffer enum '{buffer_enum}' for '{name}' in '{ip}' defined multiple times in enum header.")
            buffer_enum_value = enumVals[buffer_enum]

            if "INVALID" not in buffer_enum and buffer_enum_value not in buffer_enums:
                buffer_enums[buffer_enum_value] = (buffer_enum, name)

            texture_enum       = hfmt.texture['enum']
            if texture_enum not in enumVals:
                raise Exception(f"Texture enum '{texture_enum}' for '{name}' in '{ip}' not found in enum header.")
            if enumVals[texture_enum] is None:
                raise Exception(f"Texture enum '{texture_enum}' for '{name}' in '{ip}' defined multiple times in enum header.")
            texture_enum_value = enumVals[texture_enum]

            if "INVALID" not in texture_enum and texture_enum_value not in texture_enums:
                texture_enums[texture_enum_value] = (texture_enum, name)

        hwl[ip]['buffer_enums'] = buffer_enums
        hwl[ip]['texture_enums'] = texture_enums

    return hwl

def gen_hwl_header(data, output_file_path, namespace, ifmts: Dict[str, structs.IFmt]):
    environment = jinja2.Environment(loader=jinja2.FileSystemLoader(TEMPLATE_DIR))
    template    = environment.get_template("template_hwl.h.j2")

    context = {
        'FileName': os.path.basename(output_file_path),
        'Year': time.strftime("%Y"),
        "GfxipNamespace": namespace,
        "Data": data,
        "IFmts": ifmts,
    }

    os.makedirs(os.path.dirname(output_file_path), exist_ok=True)

    with open(output_file_path, 'w', encoding='utf8') as output:
        output.write(template.render(context))

if __name__ == "__main__":
    BASE_YAML = f"{DATA_DIR}/pal.yaml"

    parser = argparse.ArgumentParser('Script for generating format support information for PAL.')

    parser.add_argument("out_dir", help="Path to output directory.")

    args = parser.parse_args()

    ifmts = gen_base_headers(BASE_YAML, f"{args.out_dir}/src/core/g_mergedFormatInfo.h")

    GFX9 = [
        ("Gfx10",   "gfx10.yaml"),
        ("Gfx10_3", "gfx10_3.yaml"),
        ("Gfx11",   "gfx11.yaml"),
    ]
    gfx9_hwl = retrieve_hfmt_data(GFX9, ifmts)
    gen_hwl_header(gfx9_hwl, f"{args.out_dir}/src/core/hw/gfxip/gfx9/g_gfx9MergedDataFormats.h", "Gfx9", ifmts)

