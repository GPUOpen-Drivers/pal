##
 #######################################################################################################################
 #
 #  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

function(target_pal_settings TARGET)
    set(options NO_REGISTRY)
    set(singleValArgs MAGIC_BUF CLASS_NAME CODE_TEMPLATE OUT_DIR OUT_BASENAME ROOT_BINARY_DIR)
    set(multiValArgs SETTINGS ENSURE_DELETED ADDL_NAMESPACES)
    cmake_parse_arguments(PARSE_ARGV 1 SETGEN "${options}" "${singleValArgs}" "${multiValArgs}")

    # Asserts
    if (NOT SETGEN_SETTINGS)
        message(FATAL_ERROR "No settings inputs provided!")
    endif()
    if (SETGEN_OUT_BASENAME OR SETGEN_CLASS_NAME)
        list(LENGTH SETGEN_SETTINGS SETGEN_SETTINGS_LEN)
        if (${SETGEN_SETTINGS_LEN} GREATER 1)
            message(FATAL_ERROR "With provided options, only one settings file can be set!")
        endif()
    endif()
    if (SETGEN_ENSURE_DELETED)
        foreach(DELETED_FILE ${SETGEN_ENSURE_DELETED})
            get_filename_component(DELETED_FILE "${DELETED_FILE}" ABSOLUTE)
            if(EXISTS ${DELETED_FILE})
                message(FATAL_ERROR "Conflicts detected: ${DELETED_FILE} should be deleted!")
            endif()
        endforeach()
    endif()

    # Apply defaults
    if (NOT SETGEN_OUT_DIR)
        set(SETGEN_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_settings")
    endif()
    if (NOT SETGEN_CODE_TEMPLATE)
        set(SETGEN_CODE_TEMPLATE "${PAL_GEN_DIR}/settingsCodeTemplates.py")
    endif()
    get_filename_component(SETGEN_CODE_TEMPLATE "${SETGEN_CODE_TEMPLATE}" ABSOLUTE)

    # List dependencies which cause a rerun if modified (cmdline changes implicitly rerun).
    set(ADDL_DEPS
       "${PAL_GEN_DIR}/genSettingsCode.py"
       "${SETGEN_CODE_TEMPLATE}"
    )

    # Build up common args based on the provided settings
    set(ADDL_ARGS
        "--codeTemplateFile" "${SETGEN_CODE_TEMPLATE}"
        "--outDir" "${SETGEN_OUT_DIR}"
    )
    if (NOT SETGEN_NO_REGISTRY)
        list(APPEND ADDL_ARGS "--genRegistryCode")
    endif()
    if (SETGEN_CLASS_NAME)
        list(APPEND ADDL_ARGS "--classNameOverride" "${SETGEN_CLASS_NAME}")
    endif()
    if (SETGEN_ADDL_NAMESPACES)
        list(APPEND ADDL_ARGS "--additionalNamespaces" "${SETGEN_ADDL_NAMESPACES}")
    endif()

    if (SETGEN_MAGIC_BUF)
        get_filename_component(SETGEN_MAGIC_BUF "${SETGEN_MAGIC_BUF}" ABSOLUTE)
        list(APPEND ADDL_DEPS "${SETGEN_MAGIC_BUF}")
        list(APPEND ADDL_ARGS "--magicBuffer" "${SETGEN_MAGIC_BUF}")
    endif()

    # Create and include output dir
    file(MAKE_DIRECTORY ${SETGEN_OUT_DIR})
    target_include_directories(${TARGET} PRIVATE ${SETGEN_OUT_DIR})

    foreach(SETTINGS_FILE ${SETGEN_SETTINGS})
        if (SETGEN_OUT_BASENAME)
            set(OUT_BASENAME ${SETGEN_OUT_BASENAME})
        else()
            convert_pal_settings_name(${SETTINGS_FILE} OUT_BASENAME TRUE)
        endif()

        get_filename_component(SETTINGS_FILE "${SETTINGS_FILE}" ABSOLUTE)
        target_sources(${TARGET} PRIVATE
                       ${SETGEN_OUT_DIR}/${OUT_BASENAME}.h
                       ${SETGEN_OUT_DIR}/${OUT_BASENAME}.cpp
                       ${SETTINGS_FILE}
        )
        set_source_files_properties(
                       ${SETGEN_OUT_DIR}/${OUT_BASENAME}.h
                       ${SETGEN_OUT_DIR}/${OUT_BASENAME}.cpp
                       TARGET_DIRECTORY ${TARGET}
                       PROPERTIES GENERATED ON
        )

        if ((NOT EXISTS ${SETGEN_OUT_DIR}/${OUT_BASENAME}.h) OR
            (NOT EXISTS ${SETGEN_OUT_DIR}/${OUT_BASENAME}.cpp))
            # Generate these during configuration so that they are guaranteed to exist.
            execute_process(
                COMMAND ${Python3_EXECUTABLE} ${PAL_GEN_DIR}/genSettingsCode.py
                        --settingsFile ${SETTINGS_FILE}
                        --outFilename ${OUT_BASENAME}
                        ${ADDL_ARGS}
            )
        endif()

        convert_pal_settings_name(${SETTINGS_FILE} SETTING_TGT FALSE)
        add_custom_command(
            OUTPUT ${SETGEN_OUT_DIR}/${OUT_BASENAME}.cpp
                   ${SETGEN_OUT_DIR}/${OUT_BASENAME}.h
            COMMAND ${Python3_EXECUTABLE} ${PAL_GEN_DIR}/genSettingsCode.py
                    --settingsFile ${SETTINGS_FILE}
                    --outFilename ${OUT_BASENAME}
                    ${ADDL_ARGS}
            COMMENT "Generating settings from ${SETTINGS_FILE}..."
            DEPENDS ${SETTINGS_FILE}
                    ${ADDL_DEPS}
        )
        add_custom_target(${SETTING_TGT}
            DEPENDS ${SETGEN_OUT_DIR}/${OUT_BASENAME}.cpp
                    ${SETGEN_OUT_DIR}/${OUT_BASENAME}.h
            SOURCES ${SETGEN_OUT_DIR}/${OUT_BASENAME}.cpp
                    ${SETGEN_OUT_DIR}/${OUT_BASENAME}.h
        )

        add_dependencies(${TARGET} ${SETTING_TGT})
        set_target_properties(${SETTING_TGT}
            PROPERTIES
                FOLDER "${CMAKE_FOLDER}/Generate/Settings"
        )

        if (SETGEN_ROOT_BINARY_DIR)
            source_group(
                TREE ${PAL_BINARY_DIR}
                FILES
                    ${SETGEN_OUT_DIR}/${OUT_BASENAME}.h
                    ${SETGEN_OUT_DIR}/${OUT_BASENAME}.cpp
            )
        else()
            source_group("Generated/Settings"
                FILES
                    ${SETGEN_OUT_DIR}/${OUT_BASENAME}.h
                    ${SETGEN_OUT_DIR}/${OUT_BASENAME}.cpp
            )
        endif()
    endforeach()
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

