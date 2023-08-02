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
from ruamel.yaml import YAML
from os import path
from jinja2 import Environment as JinjaEnv, FileSystemLoader as JinjaLoader

def fnv1a(bytes: bytes):
    fnv_prime = 0x01000193
    hval = 0x811C9DC5
    uint32Mod = 0xFFFFFFFF

    for byte in bytes:
        hval = int(byte) ^ hval
        hval = hval * fnv_prime
        hval = hval & uint32Mod

    return hval

def buildtypes_to_c_macro(buildtypes: list) -> str:
    """A Jinja filter that transform build types to C preprocessor macros.

    For example, if builtypes is [APPLE, BANANA], the result is `#if APPLE | BANANA`
    """
    result = "#if "
    for index, build_type in enumerate(buildtypes):
        if index > 0:
            result += " || "

        build_type = build_type.strip()
        and_op_pos = build_type.find("&&")
        if and_op_pos >= 0:
            assert and_op_pos > 0 and and_op_pos < (len(build_type) - 2), "Invalid build type: {build_type}"
            result += "({build_type})"
        else:
            result += build_type
    return result

def setup_default(setting: dict, group_name: str) -> str:
    """A Jinja filter that renders settings default value assignment."""

    def assign_line(setting: dict, platform: str, group_name: str) -> str:
        setting_name = setting["variableName"]
        if len(group_name) != 0:
            setting_name = f"{group_name}.{setting_name}"

        default = setting["defaults"][platform]
        default_str = ""
        enum_name = setting.get("enum", None)
        is_string = False
        if enum_name:
            if isinstance(default, str):
                default_str = f"{enum_name}::{default}"
            elif isinstance(default, int):
                default_str = str(default)
            else:
                raise ValueError(
                    f"{setting_name} is an Enum. Its default value must be either "
                    f"an integer or a string representing one of the values of its "
                    f"Enum type, but it's {type(default)}."
                )

        elif isinstance(default, bool):
            default_str = "true" if default else "false"

        elif isinstance(default, int):
            flags = setting.get("flags", None)
            if flags and "isHex" in flags:
                default_str = hex(default)
            else:
                default_str = str(default)

        elif isinstance(default, str):
            setting_type = setting["defaults"]["type"]
            if setting_type == "float":
                default_str = default
                if default_str[-1:] != "f":
                    default_str += "f"
            elif setting_type in ["uint64", "uint32"]:
                default_str = default
            else:
                default_str = f'"{default}"'
                is_string = True

        elif isinstance(default, float):
            default_str = f"{str(default)}f"

        else:
            raise ValueError(
                f'Invalid type of default value for the setting "{setting_name}"'
            )

        if is_string:
            string_len = setting["stringLength"]
            result = (
                f'    static_assert({string_len} >= sizeof({default_str}), "string too long");\n'
                f"    strncpy(m_settings.{setting_name}, {default_str}, {string_len});\n"
            )
            return result
        else:
            return f"    m_settings.{setting_name} = {default_str};\n"

    defaults = setting["defaults"]

    default_win = defaults.get("windows", None)
    default_linux = defaults.get("linux", None)

    result = ""
    if (default_win is not None) and (default_linux is not None):
        result += "#if defined(_WIN32)\n"
        result += assign_line(setting, "windows", group_name)
        result += "#elif (__unix__)\n"
        result += assign_line(setting, "linux", group_name)
        result += "#else\n"
        result += assign_line(setting, "default", group_name)
        result += "#endif\n"
    elif default_win is not None:
        result += "#if defined(_WIN32)\n"
        result += assign_line(setting, "windows", group_name)
        result += "#else\n"
        result += assign_line(setting, "default", group_name)
        result += "#endif\n"
    elif default_linux is not None:
        result += "#if defined(__unix__)\n"
        result += assign_line(setting, "linux", group_name)
        result += "#else\n"
        result += assign_line(setting, "default", group_name)
        result += "#endif\n"
    else:
        result += assign_line(setting, "default", group_name)

    return result

# A jinja filter that converts "Type" specified in YAML to DD_SETTINGS_TYPE.
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
    elif setting_type == "uint32":
        return "DD_SETTINGS_TYPE_UINT32"
    elif setting_type == "int64":
        return "DD_SETTINGS_TYPE_INT64"
    elif setting_type == "uint64":
        return "DD_SETTINGS_TYPE_UINT64"
    elif setting_type == "float":
        return "DD_SETTINGS_TYPE_FLOAT"
    elif setting_type == "str":
        return "DD_SETTINGS_TYPE_STRING"
    else:
        return f'"Invalid value ({setting_type}) for the field defaults::type."'

# A jinja filter that converts "Type" specified in YAML to Util::ValueType from
# PAL.
def setting_type_cpp2(setting_type: str) -> str:
    if setting_type == "bool":
        return "Util::ValueType::Boolean"
    elif setting_type in ["int8", "int16", "int32"]:
        return "Util::ValueType::Int"
    elif setting_type in ["uint8", "uint16", "uint32"]:
        return "Util::ValueType::Uint"
    elif setting_type == "uint64":
        return "Util::ValueType::Uint64"
    elif setting_type == "float":
        return "Util::ValueType::Float"
    elif setting_type == "str":
        return "Util::ValueType::Str"
    else:
        return f'"Invalid value ({setting_type}) for the field defaults::type."'

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
def gen_settings_blob(settings_root: dict, magic_buf: bytes) -> bytearray:
    settings_stream = io.StringIO()
    yaml = YAML()
    yaml.default_flow_style = False
    yaml.dump(settings_root, settings_stream)

    settings_str = settings_stream.getvalue()
    settings_bytes = bytearray(settings_str.encode(encoding="utf-8"))
    settings_bytes_len = len(settings_bytes)

    if settings_root["encoded"]:
        # Fill `magic_bytes` to be as big as `settings_bytes`.
        magic_buf_len = len(magic_buf)
        repeat = settings_bytes_len // magic_buf_len
        magic_bytes = bytearray()
        if repeat == 0:
            magic_bytes.extend(magic_buf[:settings_bytes_len])
        else:
            for i in range(repeat):
                magic_bytes.extend(magic_buf)
            magic_bytes.extend(
                magic_buf[: (settings_bytes_len - (repeat * magic_buf_len))]
            )

        # Convert both byte arrays to large intergers.
        settings_int = int.from_bytes(settings_bytes, byteorder="little", signed=False)
        magic_int = int.from_bytes(magic_bytes, byteorder="little", signed=False)

        encoded_settings_int = settings_int ^ magic_int
        return encoded_settings_int.to_bytes(
            length=settings_bytes_len, byteorder="little", signed=False
        )
    else:
        return settings_bytes

def gen_setting_name_hashes(settings: list):
    """Generate name hash for each setting.

    And add it as 'nameHash' field. This function also validates setting names
    and checks for duplicates.
    """

    setting_name_set = set()  # used to detect duplicate names
    setting_name_hash_map = dict()  # used to detect hashing collision

    for setting in settings:
        name = setting["name"]
        validate_settings_name(name)

        group = setting.get("group", None)
        if group is not None:
            name = f"{group}_{name}"

        if name not in setting_name_set:
            setting_name_set.add(name)
        else:
            raise ValueError(f"Duplicate setting name: {name}")

        name_hash = fnv1a(bytes(name.encode(encoding="utf-8")))
        if name_hash not in setting_name_hash_map:
            setting_name_hash_map[name_hash] = name
        else:
            colliding_name = setting_name_hash_map[name_hash]
            raise ValueError(
                f"Hash collision detected between setting names: {name}, {colliding_name}"
            )

        setting["nameHash"] = name_hash

def prepare_enums(settings_root: dict) -> set:
    """Prepare enums in the top-level 'enums' list.

    Return a set of all enum names. ValueError exception is raised when
    duplicate names are found.
    """
    enum_names = []
    enums = settings_root.get("enums", None)
    if enums:
        for enum in enums:
            name = enum["name"]
            validate_settings_name(name)
            enum_names.append(name)

    enum_names_unique = set(enum_names)

    if len(enum_names) != len(enum_names_unique):
        duplicates = [x for x in enum_names_unique if enum_names.count(x) > 1]
        raise ValueError(f"Duplicate Enum names: {duplicates}")

    return enum_names_unique

def prepare_settings_meta(
    settings_root: dict, magic_buf: bytes, codegen_header: str, settings_header: str
):
    """Prepare settings meta data for code generation.

    Meta data are anything that's not under the "Settings" field in the YAML.

    settings_root:
        The root of Settings YAML object.

    magic_buf:
        An array of randomly generated bytes to encode the generated
        Settings blob.

    codegen_header:
        The name of the auto-generated header file by this script.

    settings_header:
        The name of the header file that contains a subclass of `SettingsBase`.
    """

    # Generating the blob before adding anything else to settings root object.
    settings_blob = gen_settings_blob(settings_root, magic_buf)

    # Compute settings blob hash. Blob hashes are used for consistency check
    # between tools and drivers. Clamp the hash value to be an uint64_t.
    settings_root["settingsBlobHash"] = (
        hash(bytes(settings_blob)) & 0xFFFF_FFFF_FFFF_FFFF
    )

    settings_root["settingsBlob"] = ",".join(map(str, settings_blob))
    settings_root["settingsBlobSize"] = len(settings_blob)

    settings_root["numSettings"] = len(settings_root["settings"])

    settings_root["codeGenHeader"] = codegen_header
    settings_root["settingsHeader"] = settings_header

    component_name = settings_root["component"]
    validate_settings_name(component_name)
    settings_root["componentNameLower"] = component_name[0].lower() + component_name[1:]

def prepare_settings_list(settings: dict, enum_name_set: set):

    group_settings_indices = []

    for setting_idx, setting in enumerate(settings):
        setting_name = setting["name"]

        # Settings 2.0 schema no longer contains 'VariableName'. It's generated
        # from 'Name'.
        setting["variableName"] = gen_variable_name(setting_name)

        # Add "variableType" field.
        enum_name = setting.get("enum", None)
        if enum_name and (enum_name not in enum_name_set):
            raise ValueError(
                f'Unknown enum name "{enum_name}" of the setting "{setting_name}". '
                "An individual setting's enum name must match one in the Enums list."
            )

        type_str = setting["defaults"]["type"]

        flags = setting.get("flags", None)

        # If a setting is an enum, its "type" must be "uint32".
        if enum_name:
            if type_str != "uint32":
                raise ValueError(
                    f"Setting {setting_name} is an enum, its type must be uint32, but its actual type is {type_str}."
                )

        if enum_name:
            is_hex = flags.get("isHex", False) if flags else False
            if is_hex:
                setting["variableType"] = "uint32_t"
            else:
                setting["variableType"] = enum_name
        else:
            if type_str == "str":
                # We only support fixed-size string.
                setting["variableType"] = "char"
            elif type_str.startswith("int") or type_str.startswith("uint"):
                setting["variableType"] = type_str + "_t"
            else:
                setting["variableType"] = type_str

        # If a string, add a field "StringLength".
        if setting["defaults"]["type"] == "str":
            if flags and (flags.get("isDir", False) or flags.get("isFile", False)):
                setting["stringLength"] = "DevDriver::kSettingsMaxPathStrLen"
            else:
                # Constant `SettingsMaxMiscStrLen` is defined in asettingsBase.h.
                setting["stringLength"] = "DevDriver::kSettingsMaxMiscStrLen"

        # Record group settings for later processing.
        if "group" in setting:
            group_settings_indices.append(setting_idx)

    # Transform group settings (settings with a 'Group' field) into subsettings.
    setting_groups = {}  # a map of group_name -> subsettings
    for idx in reversed(group_settings_indices):
        setting = settings.pop(idx)
        group_name = setting["group"]
        del setting["group"]
        subsettings = setting_groups.get(group_name, None)
        if subsettings is None:
            subsettings = []
            setting_groups[group_name] = subsettings
        subsettings.append(setting)
    # Add setting groups into settings list.
    for group_name, subsettings in setting_groups.items():
        settings.append(
            {
                "groupName": group_name,
                "groupVariableName": group_name[:1].lower() + group_name[1:],
                "subsettings": subsettings,
            }
        )

def main():
    timer_start = time.monotonic()

    parser = argparse.ArgumentParser(
        description="Generate Settings files from a YAML files."
    )

    subparsers = parser.add_subparsers(
        help="Settings codegen commands.", dest="command"
    )

    fnv1a_parser = subparsers.add_parser(
        "fnv1a", help="Hash a string using FNV1-a algorithm."
    )
    fnv1a_parser.add_argument("str", type=str, help="A string to be hashed.")

    codegen_parser = subparsers.add_parser(
        "gen", help="Generate Settings files from a YAML files."
    )
    codegen_parser.add_argument(
        "-i",
        "--input",
        required=True,
        help="A path to a Settings YAML file that conforms to Settings 2.0 schema",
    )
    codegen_parser.add_argument(
        "-g",
        "--generated-filename",
        required=True,
        help="The name for both generated header and source files.\n"
        "The final names will be prefixed with 'g_' and suffixed with '.h'\n"
        "or '.cpp' for the final filenames.",
    )
    codegen_parser.add_argument(
        "-s",
        "--settings-filename",
        required=True,
        help="The name of the header file that contains the definition\n"
        "of a specific Settings class inheriting from SettingsBase.",
    )
    codegen_parser.add_argument(
        "-o",
        "--outdir",
        required=True,
        help="The directory to put the generated files.",
    )
    codegen_parser.add_argument(
        "--classname",
        required=False,
        help="The Settings class name override. By default, it's `<component>Settings`.",
    )
    codegen_parser.add_argument(
        "--pal",
        action="store_true",
        required=False,
        help="The generated files for PAL settings need to include some dependent PAL files.",
    )
    codegen_parser.add_argument(
        "--encoded",
        action="store_true",
        required=False,
        help="If this flag is present, the generated settings blob is not encoded by a magic buffer.",
    )

    args = parser.parse_args()

    if args.command == "fnv1a":
        print(fnv1a(args.str.encode(encoding="utf-8")))
    elif args.command == "gen":
        assert not args.generated_filename.endswith(
            ".h"
        ) and not args.generated_filename.endswith(
            ".cpp"
        ), 'The argument "generated-filename" should not contain extension'

        g_header_path = path.join(args.outdir, f"g_{args.generated_filename}.h")
        g_source_path = path.join(args.outdir, f"g_{args.generated_filename}.cpp")

        running_script_dir = sys.path[0]

        with open(args.input) as file:
            yaml = YAML(typ='safe')
            settings_root = yaml.load(file)

        with open(path.join(running_script_dir, "settings_schema.yaml")) as file:
            yaml = YAML(typ='safe')
            settings_schema = yaml.load(file)

        jsonschema.validate(settings_root, schema=settings_schema)

        settings_root["palSettings"] = args.pal
        settings_root["namespace"] = "Pal" if args.pal else settings_root["component"]

        settings_root["encoded"] = args.encoded

        settings_root["className"] = (
            args.classname
            if args.classname
            else settings_root["component"] + "Settings"
        )

        with open(path.join(running_script_dir, "magic_buffer"), "rb") as magic_file:
            magic_buf = magic_file.read()

        gen_setting_name_hashes(settings_root["settings"])

        prepare_settings_meta(
            settings_root,
            magic_buf,
            f"g_{args.generated_filename}.h",
            args.settings_filename,
        )

        enum_names = prepare_enums(settings_root)
        prepare_settings_list(settings_root["settings"], enum_names)

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

        ### Render template.

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
