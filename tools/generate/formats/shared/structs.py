##
 #######################################################################################################################
 #
 #  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

from .utils import sort_human_order
from typing import List

class IFmt:
    '''
    Hardware layer independent format, with information that is common across all different generations.
    '''
    def __init__(self):
        self.name = "Undefined"
        self.channel_mask = ['X']
        self.bit_counts = [8, 0, 0, 0]
        self.bpp = 8
        self.num_components = 1
        self.compressed = False
        self.bit_count_inaccurate = False
        self.optimal_supported = True
        self.yuv_planar = False
        self.yuv_packed = False
        self.macro_pixel_packed = False
        self.depth_stencil = False
        self.versioning = None
        self.ifdefs = []
        self.private_name = None

    @classmethod
    def Default(cls):
        return IFmt()

    @classmethod
    def From_Yaml(cls, name, yaml_node):
        ifmt = IFmt()
        ifmt.name = name

        ifmt.channel_mask             = yaml_node.get("channelMask", ifmt.channel_mask)
        ifmt.bit_counts               = yaml_node.get("bitCounts", ifmt.bit_counts)
        ifmt.bpp                      = yaml_node.get("bpp", ifmt.bpp)
        ifmt.num_components           = yaml_node.get("numComponents", ifmt.num_components)
        ifmt.compressed               = yaml_node.get("compressed", ifmt.compressed)
        ifmt.bit_count_inaccurate     = yaml_node.get("bitCountInaccurate", ifmt.bit_count_inaccurate)
        ifmt.optimal_supported        = yaml_node.get("optimalSupported", ifmt.optimal_supported)
        ifmt.yuv_planar               = yaml_node.get("yuvPlanar", ifmt.yuv_planar)
        ifmt.yuv_packed               = yaml_node.get("yuvPacked", ifmt.yuv_packed)
        ifmt.macro_pixel_packed       = yaml_node.get("macroPixelPacked", ifmt.macro_pixel_packed)
        ifmt.depth_stencil            = yaml_node.get("depthStencil", ifmt.depth_stencil)
        ifmt.versioning               = yaml_node.get("versioning", ifmt.versioning)
        ifmt.ifdefs                   = yaml_node.get('ifdefs', ifmt.ifdefs)
        ifmt.private_name             = yaml_node.get('private_name', ifmt.private_name)

        return ifmt

    @property
    def bit_counts_str(self):
        return "{ " + ", ".join([f"{x:>2d}" for x in self.bit_counts]) + " }"

    @property
    def channel_mask_str(self):
        output = ""
        output += "X" if "X" in self.channel_mask else "-"
        output += "Y" if "Y" in self.channel_mask else "-"
        output += "Z" if "Z" in self.channel_mask else "-"
        output += "W" if "W" in self.channel_mask else "-"
        return output

    @property
    def format_properties(self) -> List[str]:
        output = []
        if self.bit_count_inaccurate:
            output.append("BitCountInaccurate")
        if self.compressed:
            output.append("BlockCompressed")
        if self.macro_pixel_packed:
            output.append("MacroPixelPacked")
        if self.yuv_planar:
            output.append("YuvPlanar")
        if self.yuv_packed:
            output.append("YuvPacked")

        return output

    @property
    def numeric_support(self) -> str:
        if self.depth_stencil:
            return "DepthStencil"

        if self.yuv_packed or self.yuv_planar:
            return "Yuv"

        name = self.real_format_name.lower()
        for suffix in ("Undefined", "Unorm", "Snorm", "Uscaled", "Sscaled", "Uint", "Sint", "Float", "Srgb"):
            if name.endswith(suffix.lower()):
                return suffix

        if name.startswith("_reserved"):
            return "Undefined"

        assert False, "What is this?"

    @property
    def generate_reserved(self) -> bool:
        return self.private_name is not None

    @property
    def real_format_name(self) -> str:
        if self.private_name is not None:
            return self.private_name
        else:
            return self.name

    @property
    def ifdef_str(self) -> str:
        output = ""

        if self.versioning:
            output += f"(PAL_CLIENT_INTERFACE_MAJOR_VERSION >= {self.versioning})"

        if self.ifdefs:
            if output:
                output += " && "
            output += " && ".join(self.ifdefs)

        return output

class HFmt:
    '''
    Hardware specific format, which information that differs between hardware generations.
    '''
    def __init__(self):
        self.name = ""

        self.buffer = {
            'enum': "BUF_FMT_INVALID",
            'enum_value': 0,
            'access': []
        }

        self.texture = {
            'enum': "IMG_FMT_INVALID",
            'enum_value': 0,
            'access': [],
            'filter': [],
        }

        self.cb = {
            'enum': "COLOR_INVALID",
            'num_fmt_enum': "NUMBER_FLOAT",
            'blend': False,
            'emulated_mrt': False,
        }

        self.db = {
            'z_enum': "Z_INVALID",
            's_enum': "STENCIL_INVALID",
        }

        self.msaa = False
        self.presentable = False

    @classmethod
    def Default(cls):
        return HFmt()

    @classmethod
    def From_Yaml(cls, name, yaml_node):
        hfmt = HFmt()
        hfmt.name = name

        hfmt.buffer['enum']       = yaml_node['buffer'].get('enum', hfmt.buffer['enum'])
        hfmt.buffer['enum_value'] = yaml_node['buffer'].get('enumValue', hfmt.buffer['enum_value'])
        hfmt.buffer['access']     = yaml_node['buffer'].get('access', hfmt.buffer['access'])

        hfmt.texture['enum']       = yaml_node['texture'].get('enum', hfmt.texture['enum'])
        hfmt.texture['enum_value'] = yaml_node['texture'].get('enumValue', hfmt.texture['enum_value'])
        hfmt.texture['access']     = yaml_node['texture'].get('access', hfmt.texture['access'])
        hfmt.texture['filter']     = yaml_node['texture'].get('filter', hfmt.texture['filter'])

        hfmt.cb['enum']         = yaml_node['cb'].get('enum', hfmt.cb['enum'])
        hfmt.cb['num_fmt_enum'] = yaml_node['cb'].get('numFmtEnum', hfmt.cb['num_fmt_enum'])
        hfmt.cb['blend']        = yaml_node['cb'].get('blend', hfmt.cb['blend'])
        hfmt.cb['emulated_mrt'] = yaml_node['cb'].get('emulatedMrt', hfmt.cb['emulated_mrt'])

        hfmt.db['z_enum'] = yaml_node['db'].get('zEnum', hfmt.db['z_enum'])
        hfmt.db['s_enum'] = yaml_node['db'].get('sEnum', hfmt.db['s_enum'])

        hfmt.msaa        = yaml_node.get('msaa', hfmt.msaa)
        hfmt.presentable = yaml_node.get('presentable', hfmt.presentable)

        return hfmt

    @property
    def mrt(self):
        return self.cb['enum'] != "COLOR_INVALID" or self.cb['emulated_mrt']

    @property
    def depth(self):
        return self.db['z_enum'] != 'Z_INVALID'

    @property
    def stencil(self):
        return self.db['s_enum'] != 'STENCIL_INVALID'

    def format_feature_flags(self, ifmt: IFmt, linear: bool) -> List[str]:
        flags = set()

        if 'read' in self.texture['access']:
            if linear and ifmt.compressed:
                flags.add("Copy")
            else:
                flags.add("Copy")
                flags.add("ImageShaderRead")
                flags.add("FormatConversionSrc")
        if 'write' in self.texture['access']:
            flags.add("ImageShaderWrite")
            flags.add("Copy")

            if self.mrt:
                flags.add('FormatConversionDst')
        if ifmt.numeric_support == "Srgb" and ('write' in self.texture['access'] or self.mrt):
            flags.add('FormatConversionDst')
        if 'atomics' in self.texture['access']:
            flags.add('ImageShaderAtomics')
        if 'linear' in self.texture['filter']:
            if linear is False or ifmt.compressed is False:
                flags.add('ImageFilterLinear')
        if 'minmax' in self.texture['filter'] and ifmt.compressed is False:
            flags.add('ImageFilterMinMax')
        if 'read' in self.buffer['access']:
            flags.add('MemoryShaderRead')
        if 'write' in self.buffer['access']:
            flags.add('MemoryShaderWrite')
        if 'atomics' in self.buffer['access']:
            flags.add('MemoryShaderAtomics')
        if self.mrt:
            flags.add('Copy')
            flags.add('ColorTargetWrite')
            flags.add("FormatConversion")
        if self.cb['blend']:
            flags.add('ColorTargetBlend')
        if linear is False and self.depth:
            flags.add('DepthTarget')
        if linear is False and self.stencil:
            flags.add('StencilTarget')
        if linear is False and self.msaa:
            flags.add('MsaaTarget')
        if self.presentable:
            flags.add("WindowedPresent")

        if linear and ifmt.depth_stencil:
            flags = set()
        elif linear is False and ifmt.optimal_supported is False:
            flags = set()

        return sort_human_order(list([f"FormatFeature{x}" for x in flags]))
