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

include_guard()

set(PAL_GEN_DIR ${PAL_SOURCE_DIR}/tools/generate)
find_package(Python3 3.6 QUIET REQUIRED
    COMPONENTS Interpreter
)

function(convert_pal_settings_name SETTINGS_FILE OUT_BASENAME_VAR FOR_FILE)
    # Convert input name convention to output.
    # eg, settings_core.json -> g_coreSettings

    # 1. get basename
    get_filename_component(SETTINGS_FILE_BASENAME ${SETTINGS_FILE} NAME_WE)
    # 2. split on '_' into list
    string(REPLACE "_" ";" OUT_PARTS ${SETTINGS_FILE_BASENAME})
    # 3. reverse
    list(REVERSE OUT_PARTS)
    # 4. first part goes in unmodified
    if(FOR_FILE)
        list(POP_FRONT OUT_PARTS OUT_BASENAME)
        string(PREPEND OUT_BASENAME "g_")
    else()
        set(OUT_BASENAME "")
    endif()
    # 5. remaining parts get capitalized
    foreach(OUT_PART ${OUT_PARTS})
        string(SUBSTRING ${OUT_PART} 0 1 FIRST_LETTER)
        string(SUBSTRING ${OUT_PART} 1 -1 REM_LETTERS)
        string(TOUPPER ${FIRST_LETTER} FIRST_LETTER)
        string(APPEND OUT_BASENAME "${FIRST_LETTER}${REM_LETTERS}")
    endforeach()

    set(${OUT_BASENAME_VAR} ${OUT_BASENAME} PARENT_SCOPE)
endfunction()

function(pal_gen_settings)
    # INPUT_JSON:
    #     Path to a JSON file describing all settings of a component.
    # GENERATED_FILENAME:
    #     The name of the generated C++ files. The final name will be prefixed with 'g_' and
    #     suffixed with '.h'/'.cpp'.
    # HEADER_FILE:
    #     Path to the existing C++ header file that contains the class declaration (and its methods
    #     declaration) for this settings component.
    # OUT_DIR:
    #     Path to output directory.
    # CLASS_NAME:
    #     The class name for this settings component.
    # NAMESPACES:
    #     The C++ namespace(s) within which settings are defined.
    # INCLUDE_HEADERS:
    #     Header files the generated settings file needs to '#include'. For example, a header file
    #     that contains an existing enum definition.
    # EXPERIMENTS:
    #     Optional flag to indicate to the script that this is for experiments
    set(options EXPERIMENTS)
    set(oneValueArgs INPUT_JSON GENERATED_FILENAME HEADER_FILE OUT_DIR CLASS_NAME)
    set(multiValArgs NAMESPACES INCLUDE_HEADERS)
    cmake_parse_arguments(PARSE_ARGV 0 SETTINGS "${options}" "${oneValueArgs}" "${multiValArgs}")

    if (NOT SETTINGS_INPUT_JSON)
        message(FATAL_ERROR "No settings input json file provided.")
    endif()

    set(GENERATED_HEADER_FILENAME "g_${SETTINGS_GENERATED_FILENAME}.h")
    set(GENERATED_SOURCE_FILENAME "g_${SETTINGS_GENERATED_FILENAME}.cpp")

    file(MAKE_DIRECTORY ${SETTINGS_OUT_DIR})
    target_include_directories(pal PRIVATE ${SETTINGS_OUT_DIR})

    target_sources(pal PRIVATE
        ${SETTINGS_OUT_DIR}/${GENERATED_HEADER_FILENAME}
        ${SETTINGS_OUT_DIR}/${GENERATED_SOURCE_FILENAME}
    )

    set_source_files_properties(
        ${SETTINGS_OUT_DIR}/${GENERATED_HEADER_FILENAME}
        ${SETTINGS_OUT_DIR}/${GENERATED_SOURCE_FILENAME}
        TARGET_DIRECTORY pal
        PROPERTIES GENERATED ON
    )

    if (${PAL_DEVDRIVER_PATH} STREQUAL "default")
        set(DEVDRIVER_PATH ${PAL_SOURCE_DIR}/shared/devdriver)
    else()
        set(DEVDRIVER_PATH ${PAL_DEVDRIVER_PATH})
    endif()

    if (SETTINGS_CLASS_NAME)
        list(APPEND CODEGEN_OPTIONAL_ARGS "--classname" "${SETTINGS_CLASS_NAME}")
    endif()

    list(APPEND CODEGEN_OPTIONAL_ARGS "--is-pal-settings")

    if (SETTINGS_EXPERIMENTS)
        list(APPEND CODEGEN_OPTIONAL_ARGS "--is-experiments")
    endif()

    if ((NOT EXISTS ${SETTINGS_OUT_DIR}/${GENERATED_HEADER_FILENAME}) OR
        (NOT EXISTS ${SETTINGS_OUT_DIR}/${GENERATED_SOURCE_FILENAME}))
        # Generate these during configuration so that they are guaranteed to exist.
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${DEVDRIVER_PATH}/apis/settings/codegen/settings_codegen.py
                    --input ${PAL_SOURCE_DIR}/${SETTINGS_INPUT_JSON}
                    --generated-filename ${SETTINGS_GENERATED_FILENAME}
                    --settings-filename ${SETTINGS_HEADER_FILE}
                    --outdir ${SETTINGS_OUT_DIR}
                    --namespaces ${SETTINGS_NAMESPACES}
                    --include-headers ${SETTINGS_INCLUDE_HEADERS}
                    ${CODEGEN_OPTIONAL_ARGS}
            COMMAND_ECHO STDOUT
        )
    endif()

    convert_pal_settings_name(${SETTINGS_INPUT_JSON} SETTING_TGT FALSE)
    add_custom_command(
        OUTPUT  ${SETTINGS_OUT_DIR}/${GENERATED_HEADER_FILENAME}
                ${SETTINGS_OUT_DIR}/${GENERATED_SOURCE_FILENAME}
        COMMAND ${Python3_EXECUTABLE} ${DEVDRIVER_PATH}/apis/settings/codegen/settings_codegen.py
                --input ${PAL_SOURCE_DIR}/${SETTINGS_INPUT_JSON}
                --generated-filename ${SETTINGS_GENERATED_FILENAME}
                --settings-filename ${SETTINGS_HEADER_FILE}
                --outdir ${SETTINGS_OUT_DIR}
                --namespaces ${SETTINGS_NAMESPACES}
                --include-headers ${SETTINGS_INCLUDE_HEADERS}
                ${CODEGEN_OPTIONAL_ARGS}
        COMMENT "Generating settings from ${PAL_SOURCE_DIR}/${SETTINGS_INPUT_JSON}..."
        DEPENDS ${PAL_SOURCE_DIR}/${SETTINGS_INPUT_JSON}
                ${DEVDRIVER_PATH}/apis/settings/codegen/settings_codegen.py
                ${DEVDRIVER_PATH}/apis/settings/codegen/settings_schema.yaml
                ${DEVDRIVER_PATH}/apis/settings/codegen/settings.cpp.jinja2
                ${DEVDRIVER_PATH}/apis/settings/codegen/settings.h.jinja2
    )

    add_custom_target(${SETTING_TGT}
        DEPENDS ${SETTINGS_OUT_DIR}/${GENERATED_HEADER_FILENAME}
                ${SETTINGS_OUT_DIR}/${GENERATED_SOURCE_FILENAME}
        SOURCES ${SETTINGS_OUT_DIR}/${GENERATED_HEADER_FILENAME}
                ${SETTINGS_OUT_DIR}/${GENERATED_SOURCE_FILENAME}
    )
    add_dependencies(pal ${SETTING_TGT})
    set_target_properties(${SETTING_TGT}
        PROPERTIES
            FOLDER "${CMAKE_FOLDER}/Generate/Settings"
    )

    source_group(
        TREE ${PAL_BINARY_DIR}
        FILES
            ${SETTINGS_OUT_DIR}/${GENERATED_HEADER_FILENAME}
            ${SETTINGS_OUT_DIR}/${GENERATED_SOURCE_FILENAME}
    )
endfunction()

function(pal_setup_generated_code)
    set(COMMON_ARGS "ROOT_BINARY_DIR" ${PAL_BINARY_DIR})

    pal_gen_settings(INPUT_JSON         src/core/settings_core.json
                     GENERATED_FILENAME coreSettings
                     HEADER_FILE        core/settingsLoader.h
                     OUT_DIR            ${PAL_BINARY_DIR}/src/core
                     CLASS_NAME         SettingsLoader
                     NAMESPACES         Pal)

    pal_gen_settings(INPUT_JSON         src/core/settings_platform.json
                     GENERATED_FILENAME platformSettings
                     HEADER_FILE        core/platformSettingsLoader.h
                     OUT_DIR            ${PAL_BINARY_DIR}/src/core
                     CLASS_NAME         PlatformSettingsLoader
                     NAMESPACES         Pal
                     INCLUDE_HEADERS    palDevice.h
                                        palDbgPrint.h)

    pal_gen_settings(INPUT_JSON         src/core/experiments_settings.json
                     GENERATED_FILENAME expSettings
                     HEADER_FILE        core/experimentsLoader.h
                     OUT_DIR            ${PAL_BINARY_DIR}/src/core
                     CLASS_NAME         ExperimentsLoader
                     EXPERIMENTS
                     NAMESPACES         Pal)

    if (PAL_BUILD_GFX9)
        pal_gen_settings(INPUT_JSON         src/core/hw/gfxip/gfx9/settings_gfx9.json
                         GENERATED_FILENAME gfx9Settings
                         HEADER_FILE        core/hw/gfxip/gfx9/gfx9SettingsLoader.h
                         OUT_DIR            ${PAL_BINARY_DIR}/src/core/hw/gfxip/gfx9
                         CLASS_NAME         SettingsLoader
                         NAMESPACES         Pal Gfx9
                         INCLUDE_HEADERS    core/hw/gfxip/gfxDevice.h)
    endif()

#if PAL_BUILD_GFX12
    if (PAL_BUILD_GFX12)
        pal_gen_settings(INPUT_JSON         src/core/hw/gfxip/gfx12/settings_gfx12.json
                         GENERATED_FILENAME gfx12Settings
                         HEADER_FILE        core/hw/gfxip/gfx12/gfx12SettingsLoader.h
                         OUT_DIR            ${PAL_BINARY_DIR}/src/core/hw/gfxip/gfx12
                         CLASS_NAME         SettingsLoader
                         NAMESPACES         Pal Gfx12
                         INCLUDE_HEADERS    core/hw/gfxip/gfxDevice.h
                         INCLUDE_HEADERS    core/hw/gfxip/gfx12/chip/gfx12_merged_default.h)
    endif()
#endif

    pal_gen_formats()
endfunction()

function(nongen_source_groups DIR)
    # All generated files should have an explicit source_group where they are generated.

    set(singleValArgs TARGET)
    set(multiValArgs TARGETS)
    cmake_parse_arguments(PARSE_ARGV 1 SETGEN "" "${singleValArgs}" "${multiValArgs}")

    if (DEFINED SETGEN_TARGET AND DEFINED SETGEN_TARGETS)
        message(FATAL_ERROR "TARGET and TARGETS cannot both be defined at the same time!")
    elseif (DEFINED SETGEN_TARGET)
        list(APPEND SETGEN_TARGETS ${SETGEN_TARGET})
    endif()

    foreach(TGT ${SETGEN_TARGETS})
        get_target_property(_sources ${TGT} SOURCES)
        set(_nongen_sources "")
        foreach(SOURCE ${_sources})
            get_source_file_property(_isgen "${SOURCE}" GENERATED)
            if (NOT _isgen)
                list(APPEND _nongen_sources "${SOURCE}")
            endif()
        endforeach()

        source_group(
            TREE ${DIR}/
            FILES ${_nongen_sources}
        )
    endforeach()
endfunction()

function(pal_gen_formats)
    set(FORMAT_GEN_DIR ${PAL_GEN_DIR}/formats)
    set(FORMAT_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

    set(NEEDS_CONFIG_GEN_STEP FALSE)

    set(FORMAT_INDEPENDENT_HDR "${FORMAT_OUT_DIR}/src/core/g_mergedFormatInfo.h")

    if (NOT EXISTS ${FORMAT_INDEPENDENT_HDR})
        set(NEEDS_CONFIG_GEN_STEP TRUE)
    endif()

    set(FORMAT_GFX9_HDR "${FORMAT_OUT_DIR}/src/core/hw/gfxip/gfx9/g_gfx9MergedDataFormats.h")
    if (NOT EXISTS ${FORMAT_GFX9_HDR})
        set(NEEDS_CONFIG_GEN_STEP TRUE)
    endif()

#if PAL_BUILD_GFX12
    set(FORMAT_GFX12_HDR "${FORMAT_OUT_DIR}/src/core/hw/gfxip/gfx12/g_gfx12DataFormats.h")
    if (NOT EXISTS ${FORMAT_GFX12_HDR})
        set(NEEDS_CONFIG_GEN_STEP TRUE)
    endif()
#endif

    if (NEEDS_CONFIG_GEN_STEP)
        # Generate these during configuration so that they are guaranteed to exist.
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${FORMAT_GEN_DIR}/main.py
                    ${FORMAT_OUT_DIR}
            COMMAND_ECHO STDOUT
            WORKING_DIRECTORY ${FORMAT_GEN_DIR}
        )
    endif()

    add_custom_command(
        OUTPUT  ${FORMAT_INDEPENDENT_HDR}
                ${FORMAT_GFX9_HDR}
#if PAL_BUILD_GFX12
                ${FORMAT_GFX12_HDR}
#endif
        COMMAND ${Python3_EXECUTABLE} ${FORMAT_GEN_DIR}/main.py
                ${FORMAT_OUT_DIR}
        COMMENT "Generating formats from ${FORMAT_GEN_DIR}/..."
        DEPENDS ${FORMAT_GEN_DIR}/main.py
                ${FORMAT_GEN_DIR}/data/pal.yaml
                ${FORMAT_GEN_DIR}/data/gfx10.yaml
                ${FORMAT_GEN_DIR}/data/gfx10_3.yaml
                ${FORMAT_GEN_DIR}/data/gfx11.yaml
#if PAL_BUILD_GFX12
                ${FORMAT_GEN_DIR}/data/gfx12.yaml
#endif
                ${FORMAT_GEN_DIR}/shared/structs.py
                ${FORMAT_GEN_DIR}/shared/template_hwl.h.j2
                ${FORMAT_GEN_DIR}/shared/template_independent.h.j2
                ${FORMAT_GEN_DIR}/shared/utils.py
        WORKING_DIRECTORY ${FORMAT_GEN_DIR}
    )

    add_custom_target(pal_generate_formats
        DEPENDS ${FORMAT_INDEPENDENT_HDR}
                ${FORMAT_GFX9_HDR}
#if PAL_BUILD_GFX12
                ${FORMAT_GFX12_HDR}
#endif
        SOURCES ${FORMAT_INDEPENDENT_HDR}
                ${FORMAT_GFX9_HDR}
#if PAL_BUILD_GFX12
                ${FORMAT_GFX12_HDR}
#endif
    )
    target_include_directories(pal PRIVATE ${FORMAT_OUT_DIR}/src)
    add_dependencies(pal pal_generate_formats)
    set_target_properties(pal_generate_formats
        PROPERTIES
            FOLDER "${CMAKE_FOLDER}/Generate/Formats"
    )

    source_group(
        TREE ${PAL_BINARY_DIR}
        FILES
            ${FORMAT_INDEPENDENT_HDR}
            ${FORMAT_GFX9_HDR}
#if PAL_BUILD_GFX12
            ${FORMAT_GFX12_HDR}
#endif
    )
endfunction()
