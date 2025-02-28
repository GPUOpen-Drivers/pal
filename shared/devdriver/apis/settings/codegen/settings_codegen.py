##
 #######################################################################################################################
 #
 #  Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
import io
import time
import sys
import json
import copy
import random
from os import path
from jinja2 import Environment as JinjaEnv, FileSystemLoader as JinjaLoader

def get_or_assert(map: dict, field: str, err_msg: str):
    value = map.get(field, None)
    if not value:
        raise ValueError(err_msg)
    return value

def fnv1a(bytes: bytes):
    fnv_prime = 0x01000193
    hval = 0x811C9DC5
    uint32Mod = 0xFFFFFFFF

    for byte in bytes:
        hval = int(byte) ^ hval
        hval = hval * fnv_prime
        hval = hval & uint32Mod

    return hval

def buildtypes_to_c_macro(and_build_types, or_build_types) -> str:
    """A Jinja filter that transform build types to C preprocessor macros."""
    assert (and_build_types is not None) and (or_build_types is not None)

    logical_op = " && " if and_build_types else " || "
    build_types = and_build_types if and_build_types else or_build_types

    result = "#if "
    for index, build_type in enumerate(build_types):
        if index > 0:
            result += logical_op

        build_type = build_type.strip()
        result += build_type

    return result

def setup_default(setting: dict, group_name: str) -> str:
    """A Jinja filter that renders settings default value assignment."""

    def assign_line(setting: dict, platform: str, group_name: str) -> str:
        setting_name = setting["VariableName"]
        if len(group_name) != 0:
            setting_name = f"{group_name}.{setting_name}"

        assignment_line = ""

        defaults = setting.get("Defaults", None)
        if defaults:
            setting_type = setting["Type"]
            default_str = ""
            default_value = defaults[platform]
            if setting_type == "enum":
                enum_name = setting["EnumReference"]
                if isinstance(default_value, str):
                    # If the default value provides the scope, use it instead of building it up:
                    if "::" in default_value:
                        default_str = default_value
                    else:
                        default_str = f"{enum_name}::{default_value}"
                elif isinstance(default_value, int):
                    default_str = f"({enum_name}){str(default_value)}"
                else:
                    raise ValueError(
                        f"{setting_name} is an Enum. Its default value must be either "
                        f"an integer or a string representing one of the values of its "
                        f"Enum type, but it's {type(default_value)}."
                    )
            elif isinstance(default_value, bool):
                default_str = "true" if default_value else "false"
            elif isinstance(default_value, int):
                flags = setting.get("Flags", None)
                if flags and ("IsHex" in flags or "IsBitmask" in flags):
                    default_str = hex(default_value)
                else:
                    default_str = str(default_value)
            elif isinstance(default_value, float):
                assert (
                    setting_type == "float"
                ), f'In the setting {setting_name}, "Type" field is {setting_type} but the actual type of "Defaults" are float.'

                # Print floating numbers without scientific notion. Python requires manually specifying
                # precision, so pick a large enough number 20.
                default_str = format(default_value, ".20f").rstrip("0") + "f"
            elif isinstance(default_value, str):
                flags = setting.get("Flags", None)

                if setting_type == "string":
                    # Convert `\` to `\\` in the generated C++ string literal.
                    default_str = default_value.replace("\\", "\\\\")
                elif setting_type == "float":
                    if default_value == "FLT_MAX" or default_value == "FLT_MIN":
                        default_str = default_value
                    else:
                        raise ValueError(f'In the setting {setting_name}, "Type" field is {setting_type} '
                                          'but the actual type of "Defaults" is string.')
                elif setting_type == "bool":
                    raise ValueError(f'In the setting {setting_name}, "Type" field is {setting_type} '
                                      'but the actual type of "Defaults" are string.')
                else:
                    default_str = default_value
            else:
                raise ValueError(
                    f'Invalid type of default value for the setting "{setting_name}"'
                )

            if setting_type == "string":
                string_len = setting["StringLength"]
                assignment_line = (
                    f'        static_assert({string_len} > sizeof("{default_str}"), "The string setting ({setting_name}) length exceeds the max {string_len}.");\n'
                    f'        strncpy(m_settings.{setting_name}, "{default_str}", {string_len} - 1);\n'
                    f"        m_settings.{setting_name}[{string_len} - 1] = '\\0';"
                )
            else:
                assignment_line = f"        m_settings.{setting_name} = {default_str};"
        else:
            # No "Defaults", this is an optional setting.
            if setting["Type"] == "string":
                assignment_line = f"        m_settings.{setting_name}[0] = '\\0';"
            else:
                assignment_line = (
                    f"        m_settings.{setting_name} = DevDriver::NullOpt;"
                )

        return assignment_line

    result = ""

    defaults = setting.get("Defaults", None)
    if defaults:
        default_win = defaults.get("Windows", None)
        default_linux = defaults.get("Linux", None)
        default_android = defaults.get("Android", None)

        check_count = 0

        if default_win is not None:
            result += "#if defined(_WIN32)\n"
            result += assign_line(setting, "Windows", group_name)
            check_count += 1

        if default_linux is not None:
            result += "#if " if check_count == 0 else "\n#elif "
            result += "defined(__unix__) && !defined(__ANDROID__)\n"
            result += assign_line(setting, "Linux", group_name)
            check_count += 1

        if default_android is not None:
            result += "#if " if check_count == 0 else "\n#elif "
            result += "defined(__ANDROID__)\n"
            result += assign_line(setting, "Android", group_name)
            check_count += 1

        if check_count > 0:
            if check_count < 3:
                result += "\n#else\n"
                result += assign_line(setting, "Default", group_name)
            result += "\n#endif"
        else:
            result = assign_line(setting, "Default", group_name)
    else:
        result += assign_line(setting, None, group_name)

    return result

def setting_format_string(setting_type: str) -> str:
    if setting_type == "bool":
        return "%u"
    elif setting_type in ["int8", "int16", "int32"]:
        return "%d"
    elif setting_type in ["uint8", "uint16", "uint32", "enum"]:
        return "%u"
    elif setting_type == "uint64":
        return "%llu"
    elif setting_type == "float":
        return "%.2f"
    elif setting_type == "string":
        return "%s"
    else:
        return f'"Invalid value ({setting_type}) for "Type" field."'

def gen_variable_name(name: str) -> str:
    """Generate C++ variable names.
    Try our best to convert to camelCase style.

    For example:
    "PeerMemoryEnabled" -> "peerMemoryEnabled"
    "RISSharpness" -> "risSharpness"
    "TFQ" -> "tfq"
    "VpeForceTFCalculation" -> "vpeForceTfCalculation"
    "EnableLLPC" -> "enableLlpc"
    """
    var_name = ""

    upper_case_start = -1
    upper_case_len = 0
    unprocessed_char_index = 0
    for char_index in range(len(name)):
        # Find a sequence of uppercase letters.
        if name[char_index].isupper():
            if upper_case_start == -1:

                # Process prior letters which should all be lowercase.
                if char_index > 0:
                    var_name += name[unprocessed_char_index : char_index]
                    unprocessed_char_index = char_index

                upper_case_start = char_index

            upper_case_len += 1
        else:
            if upper_case_len > 0:
                # Process previous sequence of uppercase letters.
                upper_case_end = upper_case_start + upper_case_len

                if upper_case_start == 0:
                    if upper_case_len == 1:
                        var_name += name[upper_case_start].lower()
                    else:
                        var_name += name[upper_case_start : upper_case_end - 1].lower()
                        var_name += name[upper_case_end - 1]
                else:
                    if upper_case_len == 1:
                        var_name += name[upper_case_start]
                    elif upper_case_len == 2:
                        if name[char_index].isalpha():
                            var_name += name[upper_case_start : upper_case_end]
                        else:
                            var_name += name[upper_case_start]
                            var_name += name[upper_case_start + 1].lower()
                    else:
                        # Lowercase the letters in the middle.
                        var_name += name[upper_case_start]
                        var_name += name[upper_case_start + 1 : upper_case_end - 1].lower()
                        var_name += name[upper_case_end - 1]

                unprocessed_char_index = upper_case_start + upper_case_len
                upper_case_start = -1
                upper_case_len = 0

    if unprocessed_char_index < len(name):
        # Process anything left at the end.
        if upper_case_len > 0:
            if upper_case_start == 0:
                var_name += name[upper_case_start:].lower()
            else:
                var_name += name[upper_case_start]
                if upper_case_len > 1:
                    var_name += name[upper_case_start + 1:].lower()
        else:
            var_name += name[unprocessed_char_index:]

    return var_name

def validate_settings_name(name: str):
    """Valid a "name" field in YAML object.

    The generated C++ variable names are based on `name`. So we need to make
    sure they can compile in C++.

    A name must start with an alphabetic letter and can only contain
    alphanumeric characters, plus underscore.
    """
    if not name[0].isalpha():
        raise ValueError(f'"{name}" does not start with an aphabetic letter.')
    name_cleaned = name.replace("_", "")
    if not name_cleaned.isalnum():
        raise ValueError(
            f'"{name}" contains character(s) other than alphanumeric or underscore (_).'
        )

def validate_tag(tag: str):
    """Validate a "Tag" field in JSON object.

    The maximum length of a tag string is 40. A tag must start with an
    alphabetic letter, and only contain alphanumeric characters, underscores
    ('_'), dashes ('-') and spaces (' '). There cannot be trailing spaces.
    """
    MAX_TAG_STR_LEN = 40
    if len(tag) > MAX_TAG_STR_LEN:
        raise ValueError(
            f'The tag "{tag}" exceeds the maximum lenght ({MAX_TAG_STR_LEN}).'
        )

    if not tag[0].isalpha():
        raise ValueError(f'The tag "{tag}" does not start with an aphabetic letter.')

    if tag[-1] == " ":
        raise ValueError(f'The tag "{tag}" has trailing space(s).')

    tag_cleaned = tag.replace("_", "").replace("-", "").replace(" ", "")
    if not tag_cleaned.isalnum:
        raise ValueError(
            f'The tag "{tag}" contains characters(s) other than '
            "alphanumeric underscores, dashes, and spaces"
        )

# Generate a encoded byte array of the settings json data.
def gen_settings_blob(
    settings_root: dict, magic_buf: bytes, magic_buf_start_index: int
) -> bytearray:
    settings_stream = io.StringIO()
    json.dump(settings_root, settings_stream)

    settings_str = settings_stream.getvalue()
    settings_bytes = bytearray(settings_str.encode(encoding="utf-8"))

    if magic_buf:
        # Rotate the magic buffer so it starts at `magic_buf_start_index`.
        magic_buf_len = len(magic_buf)
        magic_start_index = magic_buf_start_index % magic_buf_len
        magic_bytes = bytearray()
        magic_bytes.extend(magic_buf[magic_start_index:])
        if magic_start_index != 0:
            magic_bytes.extend(magic_buf[0:magic_start_index])

        settings_bytes_len = len(settings_bytes)
        if settings_bytes_len > magic_buf_len:
            # Extend `magic_bytes` if `settings_bytes` is longer.
            magic_bytes_copy = copy.deepcopy(magic_bytes)
            repeat = settings_bytes_len // magic_buf_len
            for i in range(repeat - 1):
                magic_bytes.extend(magic_bytes_copy)
            magic_bytes.extend(
                magic_bytes_copy[: (settings_bytes_len - (repeat * magic_buf_len))]
            )

        # Convert both byte arrays to large intergers.
        settings_int = int.from_bytes(settings_bytes, byteorder="little", signed=False)
        magic_int = int.from_bytes(
            magic_bytes[:settings_bytes_len], byteorder="little", signed=False
        )

        encoded_settings_int = settings_int ^ magic_int
        return encoded_settings_int.to_bytes(
            length=settings_bytes_len, byteorder="little", signed=False
        )
    else:
        return settings_bytes

def gen_setting_name_hashes(settings: list):
    """Generate name hash for each setting.

    And add it as 'NameHash' field. This function also validates setting names
    and checks for duplicates.
    """

    setting_name_set = set()  # used to detect duplicate names
    setting_name_hash_map = dict()  # used to detect hashing collision

    for setting in settings:
        name = setting["Name"]
        validate_settings_name(name)

        assert name not in setting_name_set, f'Duplicate setting name: "{name}".'
        setting_name_set.add(name)

        if "Structure" in setting:
            subsetting_name_set = set()  # used to detect duplicate in subsettings.

            for subsetting in setting["Structure"]:
                subsetting_name = subsetting["Name"]
                validate_settings_name(subsetting_name)

                assert (
                    subsetting_name not in subsetting_name_set
                ), f'Duplicate subsetting name "{subsetting_name}" found in Structure "{name}".'
                subsetting_name_set.add(subsetting_name)

                full_name = f"{name}.{subsetting_name}"
                name_hash = fnv1a(bytes(full_name.encode(encoding="utf-8")))

                if name_hash in setting_name_hash_map:
                    colliding_hash_name = setting_name_hash_map[name_hash]
                    raise ValueError(
                        f"Hash collision detected between setting names: {full_name}, {colliding_hash_name}"
                    )
                else:
                    subsetting["NameHash"] = name_hash
                    setting_name_hash_map[name_hash] = full_name
        else:
            name_hash = fnv1a(bytes(name.encode(encoding="utf-8")))
            if name_hash in setting_name_hash_map:
                colliding_hash_name = setting_name_hash_map[name_hash]
                raise ValueError(
                    f"Hash collision detected between setting names: {name}, {colliding_hash_name}"
                )
            else:
                setting["NameHash"] = name_hash
                setting_name_hash_map[name_hash] = name

def prepare_enums(settings_root: dict):
    """Prepare enums from the top-level enum list and individual settings.

    Return a list of all unique enums. ValueError exception is raised when
    duplicate enums are found.
    """

    SETTING_STR_DD_TYPE_MAP = {
        "bool":   "DD_SETTINGS_TYPE_BOOL",
        "int8":   "DD_SETTINGS_TYPE_INT8",
        "uint8":  "DD_SETTINGS_TYPE_UINT8",
        "int16":  "DD_SETTINGS_TYPE_INT16",
        "uint16": "DD_SETTINGS_TYPE_UINT16",
        "int32":  "DD_SETTINGS_TYPE_INT32",
        "uint32": "DD_SETTINGS_TYPE_UINT32",
        "int64":  "DD_SETTINGS_TYPE_INT64",
        "uint64": "DD_SETTINGS_TYPE_UINT64",
        "float":  "DD_SETTINGS_TYPE_FLOAT",
        "string": "DD_SETTINGS_TYPE_STRING"
    }

    SETTING_STR_PAL_TYPE_MAP = {
        "bool":   "Util::ValueType::Boolean",
        "int8":   "Util::ValueType::Int8",
        "uint8":  "Util::ValueType::Uint8",
        "int16":  "Util::ValueType::Int16",
        "uint16": "Util::ValueType::Uint16",
        "int32":  "Util::ValueType::Int32",
        "uint32": "Util::ValueType::Uint32",
        "int64":  "Util::ValueType::Int64",
        "uint64": "Util::ValueType::Uint64",
        "float":  "Util::ValueType::Float",
        "string": "Util::ValueType::Str"
    }

    def validate_enum_reference(setting: dict, enums_list):
        valid_values = setting.get("ValidValues", None)
        setting_type = setting["Type"]
        if setting_type == "enum":
            assert valid_values, f'Setting {setting["Name"]} is of enum type, but misses "ValidValues" field.'

            valid_values_name = valid_values.get("Name", None)
            values = valid_values.get("Values", None)
            if not values:
                # ValidValues doesn't define an enum, make sure it references an item from the top-level "Enums" list.
                if all(enum["Name"] != valid_values_name for enum in enums_list):
                    raise ValueError(
                        f'Setting {setting["Name"]} references an enum that does not exist in the top-level "Enums" list'
                    )

        if valid_values:
            # Since we extracted ValidValues from inside individual settings to the top-level "Enums" list, we can
            # remove them from within settings, so that all settings reference enums from the top-level Enums list,
            # making it easier for tools to parse.
            valid_values_name = valid_values.get("Name", None)
            if valid_values_name:
                del setting["ValidValues"]
                setting["EnumReference"] = valid_values_name

    def extract_enum(setting: dict, enums_list, enum_names_unique):
        """Extract enum definitions from individual settings to the top-level Enums list."""

        valid_values = setting.get("ValidValues", None)
        if valid_values and valid_values.get("IsEnum", False):
            name = valid_values.get("Name", None)
            assert (
                name is not None
            ), 'ValidValues in the setting "{}" does not have a "Name" field'.format(
                setting["Name"]
            )

            if "Values" in valid_values:
                # Presence of "Values" field means it's enum definition. Check name duplication.
                skip_gen = valid_values.get("SkipGen", False)
                if (skip_gen == False) and (name in enum_names_unique):
                    setting_name = setting["Name"]
                    raise ValueError(
                        f'The enum name "{name}" in the setting "{setting_name}" already exists.'
                    )

                and_build_types = setting.get("BuildTypes", None)
                if and_build_types is not None:
                    valid_values["BuildTypes"] = and_build_types

                or_build_types = setting.get("OrBuildTypes", None)
                if or_build_types is not None:
                    valid_values["OrBuildTypes"] = or_build_types

                enums_list.append(valid_values)
                enum_names_unique.add(name)
            else:
                # No "Values" means this is a reference to an enum defined in top-level "Enums" list.
                if name not in enum_names_unique:
                    raise ValueError(
                        f'The enum name "{name}" in the setting "{setting_name}" does not reference any item '
                         'in the top-level "Enums" list.'
                    )

    def add_cpp_type(setting, enum_list):
        """Add a "DdType" field whose value is one of DD_SETTINGS_TYPE enum variants."""
        setting_type = setting["Type"]
        if setting_type == "enum":
            enum_name = setting["EnumReference"]
            enum_size = next(enum.get("EnumSize", 32) for enum in enum_list if enum["Name"] == enum_name)
            if enum_size == 8:
                setting["DdType"] = "DD_SETTINGS_TYPE_UINT8"
                setting["PalType"] = "Util::ValueType::Uint8"
            elif enum_size == 16:
                setting["DdType"] = "DD_SETTINGS_TYPE_UINT16"
                setting["PalType"] = "Util::ValueType::Uint16"
            elif enum_size == 32:
                setting["DdType"] = "DD_SETTINGS_TYPE_UINT32"
                setting["PalType"] = "Util::ValueType::Uint32"
            elif enum_size == 64:
                setting["DdType"] = "DD_SETTINGS_TYPE_UINT64"
                setting["PalType"] = "Util::ValueType::Uint64"
            else:
                raise ValueError(f'The enum "{enum_name}" has invalid size: {enum_size}.')
        else:
            setting["DdType"] = SETTING_STR_DD_TYPE_MAP[setting_type]
            setting["PalType"] = SETTING_STR_PAL_TYPE_MAP[setting_type]

    top_level_enums = settings_root.get("Enums", [])
    top_level_enum_names = [enum["Name"] for enum in top_level_enums]

    enum_names_unique = set(top_level_enum_names)

    if len(top_level_enum_names) != len(enum_names_unique):
        duplicates = [x for x in enum_names_unique if top_level_enum_names.count(x) > 1]
        raise ValueError(f"Duplicate Enum names: {duplicates}")

    # Extract "ValidValues" from individual settings and append them to the top-level "Enums" list.
    for setting in settings_root["Settings"]:
        subsettings = setting.get("Structure", None)
        if subsettings:
            for subs in subsettings:
                extract_enum(subs, top_level_enums, enum_names_unique)
        else:
            extract_enum(setting, top_level_enums, enum_names_unique)

    for enum in top_level_enums:
        values = enum["Values"]

        # Convert enum values from hex string to int.
        for value_item in values:
            value = value_item["Value"]
            if isinstance(value, str):
                if value.startswith("0x") or value.startswith("0X"):
                    value_item["Value"] = int(value, base=16)
                else:
                    # If an enum value references other values in the enum, expand them.
                    for v in values:
                        if v["Name"] in value:
                            value = value.replace(v["Name"], str(v["Value"]))
                    try:
                        # Some clients use math expressions, try to evaluate them:
                        value_item["Value"] = eval(value)
                    except:
                        raise ValueError(f'Enum {enum["Name"]} contains value ({value_item["Name"]}) that is of invalid format. '
                                         'The valid formats are: 1. an integer 2. a string starting with "0x"/"0X" '
                                         '3. a string of math expression 4. a string containing previously defined enum values.')

        enum_base = "uint32_t"

        if "Is64Bit" in enum:
            enum_base = "uint64_t"
            print('[SettingsCodeGen][WARNING] "Is64Bit" field is deprecated, please use "EnumSize". '
                  'For example: `"EnumSize": 64`.', file=sys.stderr)
        else:
            enum_size = enum.get("EnumSize", 32)
            if enum_size == 8:
                enum_base = "uint8_t"
            elif enum_size == 16:
                enum_base = "uint16_t"
            elif enum_size == 32:
                enum_base = "uint32_t"
            elif enum_size == 64:
                enum_base = "uint64_t"
            else:
                raise ValueError(f'Enum {enum["Name"]} contains invalid "EnumSize" value ({enum_size}). '
                                  'Valid values are: 8, 16, 32, 64.')

        enum["EnumBase"] = enum_base

    for setting in settings_root["Settings"]:
        # If the setting is an enum, validate it.
        if "Structure" in setting:
            for subsetting in setting["Structure"]:
                validate_enum_reference(subsetting, top_level_enums)
        else:
            validate_enum_reference(setting, top_level_enums)

    for setting in settings_root["Settings"]:
        # If the setting is an enum, validate it.
        if "Structure" in setting:
            for subsetting in setting["Structure"]:
                add_cpp_type(subsetting, top_level_enums)
        else:
            add_cpp_type(setting, top_level_enums)

    if "Enums" not in settings_root:
        settings_root["Enums"] = top_level_enums

def prepare_settings_meta(
    settings_root: dict, magic_buf: bytes, codegen_header: str, settings_header: str
):
    """Prepare settings meta data for code generation.

    Meta data are anything that's not under the "Settings" field in the YAML.

    settings_root:
        The root of Settings JSON object.

    magic_buf:
        An array of randomly generated bytes to encode the generated
        Settings blob.

    codegen_header:
        The name of the auto-generated header file by this script.

    settings_header:
        The name of the header file that contains a subclass of `SettingsBase`.
    """

    # Generating the blob before adding anything else to settings root object.
    random.seed(int(time.time()))
    magic_buf_start = random.randint(0, 0xFFFFFFFF)
    settings_blob = gen_settings_blob(settings_root, magic_buf, magic_buf_start)
    settings_root["MagicOffset"] = magic_buf_start

    # Compute settings blob hash. Blob hashes are used for consistency check
    # between tools and drivers. Clamp the hash value to be an uint64_t.
    settings_root["SettingsBlobHash"] = (
        hash(bytes(settings_blob)) & 0xFFFF_FFFF_FFFF_FFFF
    )

    settings_root["SettingsBlob"] = ",".join(map(str, settings_blob))
    settings_root["SettingsBlobSize"] = len(settings_blob)

    settings_root["NumSettings"] = len(settings_root["Settings"])

    settings_root["CodeGenHeader"] = codegen_header
    settings_root["SettingsHeader"] = settings_header

    component_name = settings_root["ComponentName"]
    validate_settings_name(component_name)
    settings_root["ComponentNameLower"] = component_name[0].lower() + component_name[1:]

def add_variable_type(setting: dict):
    """Add a "VariableType" field to `setting`, used for generating C++ variable type."""
    setting_name = setting["Name"]
    variable_type = ""

    type_str = setting["Type"]
    flags = setting.get("Flags", None)

    if type_str == "enum":
        variable_type = setting["EnumReference"]
    elif flags and "IsBitmask" in flags:
        assert (type_str == "uint16" or type_str == "uint32" or type_str == "uint64"), \
        f'Because setting "{setting_name}" is a bitmask, its "Type" must be "uint16", "uint32" or "uint64" but it\'s {type_str}.'
        variable_type = type_str + "_t"
    elif type_str == "string":
        variable_type = "char"
        if flags and flags.get("IsDir", False):
            setting["StringLength"] = "DD_SETTINGS_MAX_PATH_SIZE"
        elif flags and flags.get("IsFile", False):
            setting["StringLength"] = "DD_SETTINGS_MAX_FILE_NAME_SIZE"
        else:
            setting["StringLength"] = "DD_SETTINGS_MAX_MISC_STRING_SIZE"
    elif type_str.startswith("int") or type_str.startswith("uint"):
        variable_type = type_str + "_t"
    elif type_str == "float" or type_str == "bool":
        variable_type = type_str
    else:
        raise ValueError(f'Setting "{setting_name}" has invalid "Type" field.')

    defaults = setting.get("Defaults", [])
    if (len(defaults) == 0) and (type_str != "string"):
        variable_type = f"DevDriver::Optional<{variable_type}>"
        setting["IsOptional"] = True

    setting["VariableType"] = variable_type

def prepare_settings_list(settings: dict, top_level_enum_list: list):
    def convert_enum_value(setting: dict, top_level_enum_list: list, value: str):
        enum_ref = setting["EnumReference"]  # "EnumReference" should have been added in prepare_enum().
        enum = next(enum for enum in top_level_enum_list if enum["Name"] == enum_ref)

        enum_value_names = [value_name.strip() for value_name in value.split("|")]
        enum_value_int = 0
        for value_name in enum_value_names:
            # If the default value has a scope on it, remove it before looking up the value:
            if "::" in value_name:
                value_name = value_name.split("::")[-1]

            # value["Value"] should have already been converted to integer in prepare_enum().
            enum_value = next((value["Value"] for value in enum["Values"] if value["Name"] == value_name), None)
            if enum_value is None:
                raise ValueError(f'Setting {setting["Name"]} is bitmask, enum or has enum values. Its default value is a string, '
                                 f'but the string does not match any one or combination of values of its referenced enum {enum_ref}.'
                                 f'Only enum value names and \'|\' are allowed in the string.')
            else:
                enum_value_int |= enum_value

        return enum_value_int

    def validate_fields(setting: dict, top_level_enum_list: list):
        if "VariableName" in setting:
            raise ValueError(f'"VariableName" field is not allowed in the setting {setting["Name"]}. '
                             'The generated C++ variable name of this setting will be derived from the "Name" field.')

        if "DependsOn" in setting:
            raise ValueError(f'"DependsOn" field found in the setting {setting["Name"]}. It is deprecated and no longer allowed.')

        defaults = setting.get("Defaults", None)
        if defaults:
            assert "Default" in defaults, f'Setting {setting["Name"]}\'s "Defaults" field is missing the '\
                                           'platform-agnostic "Default" field.'
            assert "WinDefault" not in defaults, f'"WinDefault" field found in the setting {setting["Name"]}. '\
                                                  'It is deprecated, please use "Windows" instead.'
            assert "LnxDefault" not in defaults, f'"LnxDefault" field found in the setting {setting["Name"]}. '\
                                                  'It is deprecated, please use "Linux" instead.'
            assert "AndroidDefault" not in defaults, f'"AndroidDefault" field found in the setting {setting["Name"]}. '\
                                                  'It is deprecated, please use "Android" instead.'

            setting_type = setting["Type"]
            for platform, value in defaults.items():
                if setting_type == "enum" or setting_type.startswith("int") or setting_type.startswith("uint"):
                    if isinstance(value, str):
                        is_bitmask = setting.get("Flags", {}).get("IsBitmask", False)
                        if value.startswith("0x") or value.startswith("0X"):
                            # Convert hex strings to integers.
                            defaults[platform] = int(value, base=16)
                        elif not is_bitmask and setting_type != "enum":
                            if "EnumReference" in setting:
                                raise ValueError(f'Non-bitmask setting {setting["Name"]} references an enum. '
                                                  'But its "Type" is not "enum".')
                            else:
                                try:
                                    # This could be a math expressions, so evaluate it:
                                    defaults[platform] = eval(value)
                                except SyntaxError:
                                    # Syntax error. Assume that this string is useful to the driver instead.
                                    # For example, this string could be the name of a constant variable defined in a driver
                                    # header file (the header file is #included in the generated settings cpp file).
                                    defaults[platform] = value
                        else:
                            if is_bitmask or setting_type == "enum":
                                # Convert default values from string of enum value names to integers.
                                defaults[platform] = convert_enum_value(setting, top_level_enum_list, value)
                            else:
                                raise ValueError(f'Setting {setting["Name"]} is of type {setting_type}. Its default '
                                                  'values can only be integers or strings starting with "0x".')

    for setting in settings:
        structure = setting.get("Structure", None)
        scope = setting.get("Scope", None)
        if structure:
            for subsetting in structure:
                validate_fields(subsetting, top_level_enum_list)
                subsetting["VariableName"] = gen_variable_name(subsetting["Name"])
                add_variable_type(subsetting)

                if "Scope" not in subsetting:
                    subsetting["Scope"] = scope
        else:
            validate_fields(setting, top_level_enum_list)
            add_variable_type(setting)

        setting["VariableName"] = gen_variable_name(setting["Name"])

def prepare_constants(settings_root: dict):
    """Prepare constants from the top-level constants list.

    Return a list of all constants. ValueError exception is raised for unsupported types.
    """

    def generate_plaform_def(constant: dict, platform: str) -> str:
        values = constant.get("Values", None)
        value = values[platform]
        value_str = value.replace("\\", "\\\\")
        constant_type = constant["CppType"]
        name = constant["Name"]
        assignment_line = f"{constant_type} {name}[] = \"{value_str}\";"

        return assignment_line

    def generate_def_string(constant: dict):
        constant_type = constant["Type"]
        if constant_type == "string":
            constant["CppType"] = "constexpr char"
            values = constant.get("Values", None)
            if values:
                value_win = values.get("Windows", None)
                value_linux = values.get("Linux", None)
                value_android = values.get("Android", None)

                check_count = 0
                result = ""
                if value_win is not None:
                    result += "#if defined(_WIN32)\n"
                    result += generate_plaform_def(constant, "Windows")
                    check_count += 1

                if value_linux is not None:
                    result += "#if " if check_count == 0 else "\n#elif "
                    result += "defined(__unix__) && !defined(__ANDROID__)\n"
                    result += generate_plaform_def(constant, "Linux")
                    check_count += 1

                if value_android is not None:
                    result += "#if " if check_count == 0 else "\n#elif "
                    result += "defined(__ANDROID__)\n"
                    result += generate_plaform_def(constant, "Android")
                    check_count += 1

                if check_count > 0:
                    if check_count < 3:
                        result += "\n#else\n"
                        result += generate_plaform_def(constant, "Value")
                    result += "\n#endif"
                else:
                    result = generate_plaform_def(constant, "Value")

                constant["Definition"] = result
            else:
                raise ValueError(f'{constant["Name"]} needs a value')
        else:
            raise ValueError(f'{constant["Name"]} has unsupported type. Only string constants are supported currently')

    for constant in settings_root.get("Constants", []):
        generate_def_string(constant)

def main():
    timer_start = time.monotonic()

    parser = argparse.ArgumentParser(
        description="Generate Settings files from a YAML files."
    )

    parser.add_argument(
        "-i",
        "--input",
        required=True,
        help="The path to a Settings YAML file that conforms to the version 2 of Settings schema.",
    )
    parser.add_argument(
        "-g",
        "--generated-filename",
        required=True,
        help="The name for both generated header and source files.\n"
        "The final names will be prefixed with 'g_' and suffixed with '.h'\n"
        "or '.cpp' for the final filenames.",
    )
    parser.add_argument(
        "-s",
        "--settings-filename",
        required=True,
        help="The name of the header file that contains the definition\n"
        "of a specific Settings class inheriting from SettingsBase.",
    )
    parser.add_argument(
        "-o",
        "--outdir",
        required=True,
        help="The directory to put the generated files.",
    )
    parser.add_argument(
        "--classname",
        required=False,
        help="The SettingsLoader class name override. By default, it's `<componentName>SettingsLoader`.",
    )
    parser.add_argument(
        "--settings-struct-name",
        required=False,
        help="The name of the generated C++ struct that contains all settings' definitions specified in the input JSON"
             " file. By default, it's `<ComponentName>Settings`.",
    )
    parser.add_argument(
        "--namespaces",
        nargs="*",
        default=[],
        help="C++ namespace(s) within which settings will be defined.",
    )
    parser.add_argument(
        "--include-headers",
        nargs="*",
        default=[],
        help="C++ header files that the generated settings header file needs to '#include'. For example, a header file containing an existing enum definition.",
    )
    parser.add_argument(
        "--encoded",
        action="store_true",
        required=False,
        help="If this flag is present, the generated settings blob is not encoded by a magic buffer.",
    )
    parser.add_argument(
        "--is-pal-settings",
        action="store_true",
        required=False,
        help="If this flag is present, the settings are part of PAL.",
    )

    parser.add_argument(
        "--is-experiments",
        action="store_true",
        required=False,
        help="If this flag is present, the settings are driver experiments.",
    )

    parser.add_argument(
        "--no-registry",
        action="store_true",
        required=False,
        help="If this flag is present, the read settings will not use the registry path.",
    )

    args = parser.parse_args()

    assert not args.generated_filename.endswith(
        ".h"
    ) and not args.generated_filename.endswith(
        ".cpp"
    ), 'The argument "generated-filename" should not contain extension'

    g_header_path = path.join(args.outdir, f"g_{args.generated_filename}.h")
    g_source_path = path.join(args.outdir, f"g_{args.generated_filename}.cpp")

    running_script_dir = sys.path[0]

    with open(args.input) as file:
        settings_root = json.load(file)

    settings_root["IsEncoded"] = args.encoded

    settings_root["SkipRegistry"] = args.no_registry

    settings_root["ClassName"] = (
        args.classname
        if args.classname
        else settings_root["ComponentName"] + "SettingsLoader"
    )

    settings_root["SettingsStructName"] = (
        args.settings_struct_name
        if args.settings_struct_name
        else settings_root["ComponentName"] + "Settings"
    )

    magic_buf = None
    if settings_root["IsEncoded"]:
        with open(path.join(running_script_dir, "magic_buffer.txt"), "r") as magic_file:
            magic_str = magic_file.read()
            magic_number_list = map(int, magic_str.split(","))
            magic_buf = bytes(magic_number_list)

    gen_setting_name_hashes(settings_root["Settings"])

    prepare_enums(settings_root)

    prepare_constants(settings_root)

    prepare_settings_list(settings_root["Settings"], settings_root["Enums"])

    prepare_settings_meta(
        settings_root,
        magic_buf,
        f"g_{args.generated_filename}.h",
        args.settings_filename,
    )

    settings_root["IsPalSettings"] = args.is_pal_settings

    settings_root["IsExperiments"] = args.is_experiments

    # Because PAL hasn't been updated to use `--is-pal-settings`, this if-statement needs to
    # stay to pass the rc branch's ci. Should be removed once this code is checked in PAL.
    if settings_root["ComponentName"] in ["Pal", "PalPlatform", "Gfx9Pal"]:
        settings_root["IsPalSettings"] = True

    settings_root["IsDxSettings"] = True if (settings_root["ComponentName"] == "Dxc" or settings_root["ComponentName"] == "Dxn") else False

    settings_root["Namespaces"] = args.namespaces
    settings_root["IncludeHeaders"] = args.include_headers

    # Set up Jinja Environment.

    jinja_env = JinjaEnv(
        keep_trailing_newline=True,
        # Jinja2 templates should be in the same directory as this script.
        loader=JinjaLoader(running_script_dir),
    )
    jinja_env.filters["buildtypes_to_c_macro"] = buildtypes_to_c_macro
    jinja_env.filters["setup_default"] = setup_default
    jinja_env.filters["setting_format_string"] = setting_format_string

    header_template = jinja_env.get_template("settings.h.jinja2")

    # Turn on global trim-spaces flag for the source file template.
    jinja_env.trim_blocks = True
    jinja_env.lstrip_blocks = True

    source_template = jinja_env.get_template("settings.cpp.jinja2")

    # Render template.

    with open(g_header_path, "w") as generated_file:
        generated_file.write(header_template.render(settings_root))

    with open(g_source_path, "w") as generated_file:
        generated_file.write(source_template.render(settings_root))

    execution_time = time.monotonic() - timer_start
    # Comment out printing to avoid polluting compilation output. But leave it here so we can easily check timing during development.
    # print("Settings C++ code generated successfully, in {} milliseconds.".format(execution_time * 1000))

    return 0

if __name__ == "__main__":
    sys.exit(main())
