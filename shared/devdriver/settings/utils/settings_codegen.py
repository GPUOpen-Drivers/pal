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
import json
import time
import jsonschema
import sys
from os import path
from types import CodeType
from jinja2 import Environment as JinjaEnv, FileSystemLoader as JinjaLoader
from settings_json_convert import convert_to_settings2

def fnv1a(bytes):
    fnv_prime = 0x01000193
    hval      = 0x811c9dc5
    uint32Mod = 2 ** 32
    bytes_len = len(bytes)

    # Hash 4 bytes at a time.
    byte4_end = int(bytes_len / 4) * 4
    for i in range(0, byte4_end, 4):
        int32 = int.from_bytes(bytes[i:(i+4)], byteorder='little', signed=False)
        hval = hval ^ int32
        hval = (hval * fnv_prime)

    if byte4_end < bytes_len:
        int32 = int.from_bytes(bytes[byte4_end:], byteorder='little', signed=False)
        hval = hval ^ int32
        hval = (hval * fnv_prime)

    return hval % uint32Mod

# Previously setting name-hash was used to index settings. To be backwards compatible,
# the exact same hashes need to be reproduced for each setting name. This should be
# removed when we move all settings to Settings 2.0 where setting names are used
# directly for indexing.
def fnv1a_legacy(str):
    fnv_prime = 0x01000193
    hval      = 0x811c9dc5
    uint32Max = 2 ** 32

    for c in str:
        hval = hval ^ ord(c)
        hval = (hval * fnv_prime) % uint32Max
    return hval

# A Jinja filter that concatenates multiple preprocessor conditions.
def macros_concat(prep_cond):
    macros = prep_cond['Macros']
    macros_len = len(macros)

    cond = ''
    if macros_len == 1:
        cond = macros[0]
    else:
        logic_op = '&&' if prep_cond['Relation'] == 'AND' else '||'
        for i, macro in enumerate(macros):
            cond += macro
            if i < (macros_len - 1):
                cond += f' {logic_op} '

    return cond

# A Jinja filter that renders settings default value assignment.
def setup_default(setting: dict, group_name: str) -> str:
    def assign_line(setting: dict, platform: str, group_name: str) -> str:
        setting_name = setting['VariableName']
        if len(group_name) != 0:
            setting_name = f'{group_name}.{setting_name}'

        default = setting['Defaults'][platform]
        default_str = ''
        enum_name = setting.get('Enum', None)
        bitmask_name = setting.get('Bitmask', None)
        is_string = False
        if enum_name:
            if isinstance(default, str):
                default_str = f'{enum_name}::{default}'
            elif isinstance(default, int):
                default_str = str(default)
            else:
                raise ValueError(
                    f'{setting_name} is an Enum. The type of its default value '\
                    f'must be either "int" or "str", but it\'s {type(default)}')

        elif bitmask_name:
            if isinstance(default, str):
                default_str = f'{bitmask_name}::{default}'
            elif isinstance(default, int):
                default_str = str(default)
            else:
                raise ValueError(
                    f'{setting_name} is an Bitmask. The type of its default '\
                    f'value must be either "int" or "str", but it\'s {type(default)}')

        elif isinstance(default, bool):
            default_str = 'true' if default else 'false'

        elif isinstance(default, int):
            flags = setting.get('Flags', None)
            if flags and 'IsHex' in flags:
                default_str = hex(default)
            else:
                default_str = str(default)

        elif isinstance(default, str):
            if setting['Defaults']['Type'] == 'float':
                default_str = default
                if default_str[-1:] != 'f':
                    default_str += 'f'
            else:
                default_str = f'\"{default}\"'
                is_string = True

        elif isinstance(default, float):
            default_str = f'{str(default)}f'

        else:
            raise ValueError(f'Invalid type of default value for the setting "{setting_name}"')

        if is_string:
            string_len = setting['StringLength']
            # Need to preserve this long line so the output C++ code is well formatted.
            return f'    memset(m_settings.{setting_name}, 0, {string_len});\n    strncpy(m_settings.{setting_name}, {default_str}, {string_len});\n'
        else:
            return f'    m_settings.{setting_name} = {default_str};\n'

    defaults = setting['Defaults']

    default_win = defaults.get('Windows', None)
    default_linux = defaults.get('Linux', None)

    result = ''
    if default_win and default_linux:
        result += '#if defined(_WIN32)\n'
        result += assign_line(setting, 'Windows', group_name)
        result += '#elif (__unix__)\n'
        result += assign_line(setting, 'Linux', group_name)
        result += '#else\n'
        result += assign_line(setting, 'Default', group_name)
        result += '#endif\n'
    elif default_win:
        result += '#if defined(_WIN32)\n'
        result += assign_line(setting, 'Windows', group_name)
        result += '#else\n'
        result += assign_line(setting, 'Default', group_name)
        result += '#endif\n'
    elif default_linux:
        result += '#if defined(__unix__)\n'
        result += assign_line(setting, 'Linux', group_name)
        result += '#else\n'
        result += assign_line(setting, 'Default', group_name)
        result += '#endif\n'
    else:
        result += assign_line(setting, 'Default', group_name)

    return result

# A jinja filter that returns "Util::ValueType".
def setting_type_cpp(setting_type: str) -> str:
    if setting_type == 'bool':
        return 'Util::ValueType::Boolean'
    elif setting_type == 'int8' or setting_type == 'int16' or setting_type == 'int32':
        return 'Util::ValueType::Int'
    elif setting_type == 'uint8':
        return 'Util::ValueType::Uint8'
    elif setting_type == 'uint16' or setting_type == 'uint32':
        return 'Util::ValueType::Uint'
    elif setting_type == 'uint64':
        return 'Util::ValueType::Uint64'
    elif setting_type == 'float':
        return 'Util::ValueType::Float'
    elif setting_type == 'string':
        return 'Util::ValueType::Str'
    else:
        return '"Invalid value for the JSON field Defaults::Type."'

# A jinja filter that returns "SettingType".
def setting_type_cpp2(setting_type: str) -> str:
    if setting_type == 'bool':
        return 'SettingType::Boolean'
    elif setting_type == 'int8' or setting_type == 'int16' or setting_type == 'int32':
        return 'SettingType::Int'
    elif setting_type == 'uint8':
        return 'SettingType::Uint8'
    elif setting_type == 'uint16' or setting_type == 'uint32':
        return 'SettingType::Uint'
    elif setting_type == 'uint64':
        return 'SettingType::Uint64'
    elif setting_type == 'int64':
        return 'SettingType::Int64'
    elif setting_type == 'float':
        return 'SettingType::Float'
    elif setting_type == 'string':
        return 'SettingType::String'
    else:
        return '"Invalid value for the JSON field Defaults::Type."'

# Hash the first 64 bytes of the buffer.
def gen_magic_buffer_id(magic_buf: bytes) -> bytes:
    return fnv1a(magic_buf[:64])

# Generate a encoded byte array of the settings json data.
def gen_settings_json_binary(settings: dict, magic_buf: bytes) -> bytearray:
    settings_str = json.dumps(settings, indent=None)
    settings_bytes = bytearray(settings_str.encode())

    # Pad settings_bytes to be 4-byte aligned.
    settings_bytes_len = len(settings_bytes)
    assert settings_bytes_len != 0, "Settings JSON binary buffer is empty."

    remainder4 = settings_bytes_len % 4
    if remainder4 != 0:
        for i in range(4 - remainder4):
            settings_bytes.append(0)
    settings_4byte_len = len(settings_bytes)

    # 4-byte aligned length
    magic_4byte_len = int(len(magic_buf) / 4) * 4

    # XOR all bytes of settings with that of the magic buffer.
    settings_bytes_encrypted = bytearray()
    settings_bytes_cnt = 0
    repeat = (settings_4byte_len - 1 + magic_4byte_len) % magic_4byte_len
    for _ in range(repeat):
        for i in range(0, magic_4byte_len, 4):
            if settings_bytes_cnt >= settings_4byte_len:
                break

            # Do it 4 bytes at a time.
            magic_int32 = int.from_bytes(magic_buf[i:(i+4)], byteorder='little', signed=False)
            settings_int32 = int.from_bytes(settings_bytes[settings_bytes_cnt:(settings_bytes_cnt+4)],
                                            byteorder='little', signed=False)
            xor_res = settings_int32 ^ magic_int32
            settings_bytes_encrypted.extend(xor_res.to_bytes(4, byteorder='little', signed=False))
            settings_bytes_cnt += 4

        if settings_bytes_cnt >= settings_4byte_len:
            break

    return settings_bytes_encrypted[:settings_bytes_len]

# Copy-pasted from genSettingsCode.py for parsing old Sttings json files.
def prepare_legacy_settings_json(old_settings: dict):
    visited = []
    queue   = [ { "parent": None, "item": old_settings } ]

    while len(queue) > 0:
        data   = queue.pop()
        parent = data['parent']
        item   = data['item']

        visited.append(item)

        if isinstance(item, dict):
            for key, value in item.items():
                if value not in visited:
                    queue.append({"parent": item, "item": value})
        elif isinstance(item, list):
            for value in item:
                if value not in visited:
                    queue.append({"parent": item, "item": value})

    # It's possible that the Tag's array contains both objects and strings.
    # We want to compress this array to just an array of strings.
    if "Tags" in old_settings:
        tags = old_settings["Tags"]
        for i in range(0, len(tags)):
            tag = tags[i]
            if isinstance(tag, dict):
                tags[i] = tag['Name']

    # The HashName field is required by the tool, we calculate it here as an fnv1a hash of the Name field
    # for each setting.
    for setting in old_settings["Settings"]:
        if "HashName" in setting:
            print("Setting {} already has a HashName field defined".format(setting["Name"]))
        else:
            setting["HashName"] = fnv1a_legacy(setting["Name"])
        if "Structure" in setting:
            for field in setting["Structure"]:
                if "HashName" in field:
                    print("Setting {}.{} already has a HashName field defined".format(setting["Name"], field["Name"]))
                else:
                    field["HashName"] = fnv1a_legacy(setting["Name"]+"."+field["Name"])

    # Before we convert the settings JSON to a byte array, we will strip out unnecessary whitespace and the keys
    # that are not used by the tool.
    removedKeyList = [ "VariableName", "Scope", "Preprocessor", "BuildTypes", "OrBuildTypes" ]
    for setting in old_settings:
        for key in removedKeyList:
            if key in setting: setting.pop(key)

    return old_settings

# Generate encrypted bytearray for old Settings JSON data.
def gen_legacy_settings_json_binary(legacy_settings_json, magic_buf: bytes) -> bytearray:
    settings = prepare_legacy_settings_json(legacy_settings_json)
    return gen_settings_json_binary(settings, magic_buf)

# Create a set that contains the names of all Enums in the settings JSON.
# ValueError except is raised when duplicate names are found.
def create_enum_name_set(settings_root: dict) -> set:
    enum_names = [enum['Name'] for enum in settings_root['Enums']]
    enum_names_unique = set(enum_names)

    if len(enum_names) != len(enum_names_unique):
        duplicates = [x for x in enum_names_unique if enum_names.count(x) > 1]
        raise ValueError(f'Duplicate Enum names: {duplicates}')

    return enum_names_unique

# Create a set that contains the names of all Bitmasks in the settings JSON.
# ValueError except is raised when duplicate names are found.
def create_bitmask_name_set(settings_root: dict) -> set:
    bitmask_names = [bitmask['Name'] for bitmask in settings_root['Bitmasks']]
    bitmask_names_unique = set(bitmask_names)

    if len(bitmask_names) != len(bitmask_names_unique):
        duplicates = [x for x in bitmask_names_unique if bitmask_names.count(x) > 1]
        raise ValueError(f'Duplicate Enum names: {duplicates}')

    return bitmask_names_unique

def prepare_settings_meta(
    settings_root: dict,
    magic_buf: bytes,
    legacy_settings_json: dict,
    codegen_header: str,
    settings_header: str,
    use_rpc: bool):
    """Prepare settings meta data for code generation.

    Meta data are anything that's not under the "Settings" field in the JSON.

    settings_root:
        The root of Settings JSON object.

    magic_buf:
        An array of randomly generated bytes.

    legacy_settings_json:
        A JSON object containing legacy Settings data.

    codegen_header:
        The name of the auto-generated header file by this script.

    settings_header:
        The name of the header file that contains a subclass of `SettingsBase`.

    use_rpc:
        A flag indicating whether we generate code for the RPC types.
    """

    settings_root['NumSettings'] = len(settings_root['Settings'])

    settings_root['MagicBufferId'] = gen_magic_buffer_id(magic_buf)

    settings_legacy_json_binary = gen_legacy_settings_json_binary(legacy_settings_json, magic_buf)
    settings_root['JsonDataSize_Legacy'] = len(settings_legacy_json_binary)
    settings_root['JsonDataHash_Legacy'] = fnv1a(settings_legacy_json_binary)

    settings_json_byte_str = []
    for byte in settings_legacy_json_binary:
        settings_json_byte_str.append(str(byte))
    settings_root['JsonData_Legacy'] = ', '.join(settings_json_byte_str)

    settings_root['CodeGenHeader'] = codegen_header
    settings_root['SettingsHeader'] = settings_header

    component_name = settings_root['ComponentName']
    settings_root['ComponentNameLower'] = component_name[:1].lower() + component_name[1:]

    settings_root['UseRpc'] = use_rpc

def prepare_settings_list(settings: dict, enum_name_set: set, bitmask_name_set: set):
    setting_name_set = set()

    for setting in settings:
        setting_name = setting['Name']

        if setting_name not in setting_name_set:
            setting_name_set.add(setting_name)
        else:
            raise ValueError(f'Duplicate setting name: {setting_name}')

        setting['NameHash'] = fnv1a_legacy(setting_name)

        # Add "VariableName" field.
        if not 'VariableName' in setting:
            setting['VariableName'] = setting_name[:1].lower() + setting_name[1:]

        # Add "VariableType" field.
        enum_name = setting.get('Enum', None)
        if enum_name and (enum_name not in enum_name_set):
            raise ValueError(
                f'Unknown enum name "{enum_name}" of the setting "{setting_name}". '\
                'An individual setting\'s enum name must match one in the Enums list.')

        bitmask_name = setting.get('Bitmask', None)
        if bitmask_name and (bitmask_name not in bitmask_name_set):
            raise ValueError(
                f'Unknown bitmask name "{bitmask_name} of the setting "{setting_name}". '\
                'An individual setting\'s bitmask name must match one in the Bitmasks list.')

        if enum_name:
            setting['VariableType'] = enum_name
        elif bitmask_name:
            setting['VariableType'] = 'uint32_t'
        else:
            defaults = setting['Defaults']
            type_str = defaults['Type']
            if type_str == 'string':
                # We only support fixed-size string.
                setting['VariableType'] = 'char'
            elif type_str.startswith('int') or type_str.startswith('uint'):
                setting['VariableType'] = type_str + '_t'
            else:
                setting['VariableType'] = type_str

        # If a string, add a field "StringLength".
        if setting['Defaults']['Type'] == 'string':
            flags = setting['Flags']
            if flags.get('IsDir', False) or flags.get('IsFile', False):
                setting['StringLength'] = 'DevDriver::kSettingsMaxPathStrLen'
            else:
                # Constant `SettingsMaxMiscStrLen` is defined in asettingsBase.h.
                setting['StringLength'] = 'DevDriver::kSettingsMaxMiscStrLen'

def main():
    timer_start = time.process_time_ns()

    parser = argparse.ArgumentParser(
        description='Load a legacy JSON file and convert it to Settings 2.0 \
                     schema. Then validate the converted settings JSON and \
                     generate Settings C++ header and source files.')
    parser.add_argument('-i', '--input',
                        required=True,
                        help='A path to a Settings JSON file whose schema dates \
                              before Settings 2.0.')
    parser.add_argument('-g', '--generated-filename',
                        required=True,
                        help='The name for both generated header and source files. \
                              It will be prefixed with \"g_\" and suffixed with \
                              \".h\" or \".cpp\" for the final filenames.')
    parser.add_argument('-s', '--settings-filename',
                        required=True,
                        help='The name of the header file that contains the definition \
                              of a specific Settings class inheriting from SettingsBase.')
    parser.add_argument('-o', '--outdir',
                        required=True,
                        help='The directory to put the generated files.')
    parser.add_argument('-r', '--use-rpc',
                        required=False,
                        help='Generate code only for RPC (this is only relevant to the \
                              component registration code).',
                        action='store_true')

    args = parser.parse_args()

    assert not args.generated_filename.endswith('.h') and \
           not args.generated_filename.endswith('.cpp'), \
        'The argument \"generated-filename\" should not contain extension'

    g_header_path = path.join(args.outdir, f'g_{args.generated_filename}.h')
    g_source_path = path.join(args.outdir, f'g_{args.generated_filename}.cpp')

    running_script_dir = sys.path[0]

    settings_json_legacy = None
    with open(args.input) as file:
        settings_json_legacy = json.load(file)

    settings_json = convert_to_settings2(settings_json_legacy)

    settings_schema = None
    with open(path.join(running_script_dir, 'settings_schema.json')) as file:
        settings_schema = json.load(file)

    jsonschema.validate(settings_json, schema=settings_schema)

    magic_buf = None
    with open(path.join(running_script_dir, 'magic_buffer'), 'r+b') as magic_file:
        magic_buf = magic_file.read()

    prepare_settings_meta(
        settings_json,
        magic_buf,
        settings_json_legacy,
        f'g_{args.generated_filename}.h',
        args.settings_filename,
        args.use_rpc
    )
    prepare_settings_list(
        settings_json['Settings'],
        create_enum_name_set(settings_json),
        create_bitmask_name_set(settings_json)
    )

    ### Set up Jinja Environment.

    jinja_env = JinjaEnv(trim_blocks=True,
                        lstrip_blocks=True,
                        keep_trailing_newline=True,
                        # Jinja2 templates should be in the same directory as this script.
                        loader=JinjaLoader(running_script_dir))
    jinja_env.filters['macros_concat'] = macros_concat
    jinja_env.filters['setup_default'] = setup_default
    jinja_env.filters['setting_type_cpp'] = setting_type_cpp
    jinja_env.filters['setting_type_cpp2'] = setting_type_cpp2

    header_template = jinja_env.get_template("settings.h.jinja2")
    source_template = jinja_env.get_template("settings.cpp.jinja2")

    ### Render template.

    with open(g_header_path, 'w') as generated_file:
        generated_file.write(header_template.render(settings_json))

    with open(g_source_path, 'w') as generated_file:
        generated_file.write(source_template.render(settings_json))

    execution_time = time.process_time_ns() - timer_start
    print('Settings C++ code generated successfully, in {} milliseconds.'.format(execution_time / 1000000))

    return 0

if __name__ == '__main__':
    sys.exit(main())
