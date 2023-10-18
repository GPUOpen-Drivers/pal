##
 #######################################################################################################################
 #
 #  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
import jsonschema
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
    """A Jinja filter that transform build types to C preprocessor macros.
    """
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
                enum_name = setting["ValidValues"]["Name"]
                if isinstance(default_value, str):
                    default_str = f'{enum_name}::{default_value}'
                elif isinstance(default_value, int):
                    default_str = f'({enum_name}){str(default_value)}'
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
                default_str = f'{str(default_value)}f'
            elif isinstance(default_value, str):
                if setting_type == "float":
                    default_str = default_value
                    if default_str[-1:] == "f":
                        default_str += "f"
                else:
                    flags = setting.get("Flags", None)
                    if flags and ("IsDir" in flags or "IsFile" in flags):
                        # `\` in paths cannot be an escape character. Convert `\` to `\\` in the generated C++ string literal.
                        default_str = default_value.replace('\\', '\\\\')
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
                assignment_line = f"        m_settings.{setting_name} = DevDriver::NullOpt;"

        return assignment_line

    result = ""

    defaults = setting.get("Defaults", None)
    if defaults:
        default_win = defaults.get("Windows", None)
        default_linux = defaults.get("Linux", None)
        default_android = defaults.get("Android", None)

        check_count = 0

        if default_win:
            result += "#if defined(_WIN32)\n"
            result += assign_line(setting, "Windows", group_name)
            check_count += 1

        if default_linux:
            result += "#if " if check_count == 0 else "\n#elif " + "defined(__unix__)\n"
            result += assign_line(setting, "Linux", group_name)
            check_count += 1

        if default_android:
            result += "#if " if check_count == 0 else "\n#elif " + "defined(__ANDROID__)\n"
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

# A jinja filter that converts "Type" specified in JSON to DD_SETTINGS_TYPE.
def setting_type_cpp(setting_type: str) -> str:
    if setting_type == "bool":
        return "DD_SETTINGS_TYPE_BOOL"
    elif setting_type == "int8":
        return "DD_SETTINGS_TYPE_INT8"
    elif setting_type == "uint8":
        return "DD_SETTINGS_TYPE_UINT8"
    elif setting_type == "int16":
        return "DD_SETTINGS_TYPE_INT16"
    elif setting_type == "uint16":
        return "DD_SETTINGS_TYPE_UINT16"
    elif setting_type == "int32":
        return "DD_SETTINGS_TYPE_INT32"
    elif setting_type == "uint32" or setting_type == "enum":
        return "DD_SETTINGS_TYPE_UINT32"
    elif setting_type == "int64":
        return "DD_SETTINGS_TYPE_INT64"
    elif setting_type == "uint64":
        return "DD_SETTINGS_TYPE_UINT64"
    elif setting_type == "float":
        return "DD_SETTINGS_TYPE_FLOAT"
    elif setting_type == "string":
        return "DD_SETTINGS_TYPE_STRING"
    else:
        return f'"Invalid value ({setting_type}) for the field `Type`."'

# A jinja filter that converts "Type" specified in JSON to Util::ValueType from
def setting_type_cpp2(setting_type: str) -> str:
    if setting_type == "bool":
        return "Util::ValueType::Boolean"
    elif setting_type in ["int8", "int16", "int32"]:
        return "Util::ValueType::Int"
    elif setting_type in ["uint8", "uint16", "uint32", "enum"]:
        return "Util::ValueType::Uint"
    elif setting_type == "uint64":
        return "Util::ValueType::Uint64"
    elif setting_type == "float":
        return "Util::ValueType::Float"
    elif setting_type == "string":
        return "Util::ValueType::Str"
    else:
        return f'"Invalid value ({setting_type}) for "Type" field."'

def gen_variable_name(name: str) -> str:
    """Generate C++ variable names.
    The variable name is generated by lowercasing the first character if
    it's followed by a lowercase character. If `name` starts with multiple
    uppercase characters, the first n-1 characters are lowercased, unless
    all characters in `name` are uppercase, in which case all are lowercased.

    For example:
    "PeerMemoryEnabled" -> "peerMemoryEnabled"
    "RISSharpness" -> "risSharpness"
    "TFQ" -> "tfq"
    """
    uppercase_len = 0
    for i in range(len(name)):
        if name[i].isupper():
            uppercase_len += 1
        else:
            break

    var_name = None
    if uppercase_len == len(name):
        var_name = name.lower()
    elif uppercase_len == 0:
        var_name = name
    elif uppercase_len == 1:
        var_name = name[0].lower() + name[1:]
    else:
        var_name = name[: uppercase_len - 1].lower() + name[uppercase_len - 1 :]

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
def gen_settings_blob(settings_root: dict, magic_buf: bytes, magic_buf_start_index: int) -> bytearray:
    settings_stream = io.StringIO()
    json.dump(settings_root, settings_stream)

    settings_str = settings_stream.getvalue()
    settings_bytes = bytearray(settings_str.encode(encoding="utf-8"))
    settings_bytes_len = len(settings_bytes)

    if magic_buf:
        # Rotate the magic buffer so it starts at `magic_buf_start_index`.
        magic_buf_len = len(magic_buf)
        magic_start_index = magic_buf_start_index % magic_buf_len
        magic_bytes = bytearray()
        magic_bytes.extend(magic_buf[magic_start_index:])
        if magic_start_index != 0:
            magic_bytes.extend(magic_buf[0:magic_start_index])

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
        magic_int = int.from_bytes(magic_bytes[:settings_bytes_len], byteorder="little", signed=False)

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

        if "Structure" in setting:
            subsetting_name_set = set() # used to detect duplicate in subsettings.
            subsetting_name_hash_map = dict() # used to detect duplicate in subsettings.

            for subsetting in setting["Structure"]:
                subsetting_name = subsetting["Name"]
                validate_settings_name(subsetting_name)

                assert subsetting_name not in subsetting_name_set, f'Duplicate subsetting name "{subsetting_name}" found in Structure "{name}".'
                subsetting_name_set.add(subsetting_name)

                full_name = f'{name}.{subsetting_name}'
                name_hash = fnv1a(bytes(full_name.encode(encoding="utf-8")))

                if name_hash in subsetting_name_hash_map:
                    colliding_hash_name = subsetting_name_hash_map[name_hash]
                    raise ValueError(f'"{subsetting_name}" and "{colliding_hash_name}" in the Structure "{name}" have the same name hash.')
                else:
                    subsetting["NameHash"] = name_hash
                    subsetting_name_hash_map[name_hash] = subsetting_name

            setting_name_set.add(name)
            # Don't need to add `name` to `setting_name_hash_map` since Structure name is not
            # directly hashed.
        else:
            assert name not in setting_name_set, f'Duplicate setting name: "{name}".'
            setting_name_set.add(name)

            name_hash = fnv1a(bytes(name.encode(encoding="utf-8")))
            if name_hash in setting_name_hash_map:
                colliding_hash_name = setting_name_hash_map[name_hash]
                raise ValueError(f'Hash collision detected between setting names: {name}, {colliding_hash_name}')
            else:
                setting["NameHash"] = name_hash
                setting_name_hash_map[name_hash] = name

def prepare_enums(settings_root: dict):
    """Prepare enums from the top-level enum list and individual settings.

    Return a list of all unique enums. ValueError exception is raised when
    duplicate enums are found.
    """

    def validate_enum_name(name: str):
        if not name[0].isalpha() or not name[0].isupper():
            raise ValueError(f'Enum name ("{name}") must start with an upper-case alphabetic letter.')
        if not name.isalnum():
            raise ValueError(f'Enum name ("{name}") can only have alphanumeric characters.')

    def validate_enum_reference(setting: dict, enums_list):
        setting_type = setting["Type"]
        if setting_type == "enum":
            valid_values = setting.get("ValidValues")
            setting_name = setting["Name"]
            assert valid_values, f'Setting "{setting_name}" is an enum, but misses "ValidValues" field.'

            valid_values_name = valid_values.get("Name", None)
            values = valid_values.get("Values", None)
            if not values:
                # ValidValues doesn't define an enum, make sure it references an item from the top-level "Enums" list.
                if all(enum["Name"] != valid_values_name for enum in enums_list):
                    raise ValueError(f'Setting "{setting_name}" references an enum that does not exist in the top-level "Enums" list')

    enum_names = []
    enums = settings_root.get("Enums", [])
    for enum in enums:
        name = enum["Name"]
        validate_enum_name(name)
        enum_names.append(name)

    enum_names_unique = set(enum_names)

    if len(enum_names) != len(enum_names_unique):
        duplicates = [x for x in enum_names_unique if enum_names.count(x) > 1]
        raise ValueError(f"Duplicate Enum names: {duplicates}")

    # Extract "ValidValues" from individual settings and append them to the top-level "Enums" list.
    for setting in settings_root["Settings"]:
        valid_values = setting.get("ValidValues", None)
        if valid_values and valid_values.get("IsEnum", False):

            name = valid_values.get("Name", None)
            assert name is not None, \
                'ValidValues in the setting "{}" does not have a "Name" field'.format(setting["Name"])

            values = valid_values.get("Values", None)
            if values:
                # Presence of "Values" field means it's enum definition. Check name duplication.
                if name in enum_names_unique:
                    setting_name = setting["Name"]
                    raise ValueError(f'The enum name "{name}" in the setting "{setting_name}" already exists.')

                enums.append(valid_values)
                enum_names_unique.add(name)
            else:
                # No "Values" means this is a reference to an enum defined in top-level "Enums" list.
                if name not in enum_names_unique:
                    raise ValueError(f'The enum name "{name}" in the setting "{setting_name}" does not reference any item in the top-level "Enums" list.')

    for setting in settings_root["Settings"]:
        # If the setting is an enum, validate it.
        if "Structure" in setting:
            for subsetting in setting["Structure"]:
                validate_enum_reference(subsetting, enums)
        else:
            validate_enum_reference(setting, enums)

    if "Enums" not in settings_root:
        settings_root["Enums"] = enums

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
    magic_buf_start = random.randint(0, 0xffffffff)
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
    """ Add a "VariableType" field to `setting`, used for generating C++ variable type.
    """
    setting_name = setting["Name"]
    variable_type = ""

    type_str = setting["Type"]
    flags = setting.get("Flags", None)

    if type_str == "enum":
        valid_values = setting["ValidValues"]
        variable_type = valid_values["Name"]
    elif flags and "IsBitmask" in flags:
        assert type_str == "uint32", f'Because setting "{setting_name}" is a bitmask, its "Type" must be "uint32".'
        variable_type = "uint32_t"
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

    setting["RawVariableType"] = variable_type

    defaults = setting.get("Defaults", [])
    if (len(defaults) == 0) and (type_str != "string"):
        variable_type = f"DevDriver::Optional<{variable_type}>"
        setting["IsOptional"] = True

    setting["VariableType"] = variable_type

def prepare_settings_list(settings: dict):
    for setting in settings:
        structure = setting.get("Structure", None)
        scope = setting.get("Scope", None)
        if structure:
            for subsetting in structure:
                # Settings 2.0 schema no longer contains 'VariableName'. It's generated
                # from 'Name'.
                subsetting["VariableName"] = gen_variable_name(subsetting["Name"])
                add_variable_type(subsetting)

                if "Scope" not in subsetting:
                    subsetting["Scope"] = scope
        else:
            add_variable_type(setting)

        setting["VariableName"] = gen_variable_name(setting["Name"])

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
        help="The Settings class name override. By default, it's `<component>Settings`.",
    )
    parser.add_argument(
        "--pal",
        action="store_true",
        required=False,
        help="The generated files for PAL settings need to include some dependent PAL files.",
    )
    parser.add_argument(
        "--encoded",
        action="store_true",
        required=False,
        help="If this flag is present, the generated settings blob is not encoded by a magic buffer.",
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

    settings_root["IsPalSettings"] = args.pal
    settings_root["Namespace"] = "Pal" if args.pal else settings_root["ComponentName"]

    settings_root["IsEncoded"] = args.encoded

    settings_root["ClassName"] = (
        args.classname
        if args.classname
        else settings_root["ComponentName"] + "SettingsLoader"
    )

    magic_buf = None
    if settings_root["IsEncoded"]:
        with open(path.join(running_script_dir, "magic_buffer.txt"), "r") as magic_file:
            magic_str = magic_file.read()
            magic_number_list = map(int, magic_str.split(","))
            magic_buf = bytes(magic_number_list)

    gen_setting_name_hashes(settings_root["Settings"])

    prepare_settings_meta(
        settings_root,
        magic_buf,
        f"g_{args.generated_filename}.h",
        args.settings_filename,
    )

    prepare_enums(settings_root)
    prepare_settings_list(settings_root["Settings"])

    ### Set up Jinja Environment.

    jinja_env = JinjaEnv(
        trim_blocks=True,
        lstrip_blocks=True,
        keep_trailing_newline=True,
        # Jinja2 templates should be in the same directory as this script.
        loader=JinjaLoader(running_script_dir),
    )
    jinja_env.filters["buildtypes_to_c_macro"] = buildtypes_to_c_macro
    jinja_env.filters["setup_default"] = setup_default
    jinja_env.filters["setting_type_cpp"] = setting_type_cpp
    jinja_env.filters["setting_type_cpp2"] = setting_type_cpp2

    header_template = jinja_env.get_template("settings.h.jinja2")
    source_template = jinja_env.get_template("settings.cpp.jinja2")

    # ### Render template.

    with open(g_header_path, "w") as generated_file:
        generated_file.write(header_template.render(settings_root))

    with open(g_source_path, "w") as generated_file:
        generated_file.write(source_template.render(settings_root))

    execution_time = time.monotonic() - timer_start
    print(
        "Settings C++ code generated successfully, in {} milliseconds.".format(
            execution_time * 1000
        )
    )

    return 0

if __name__ == "__main__":
    sys.exit(main())
