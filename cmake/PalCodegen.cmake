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

include_guard()

set(PAL_GEN_DIR ${PAL_SOURCE_DIR}/tools/generate)
find_package(Python3 3.6 QUIET REQUIRED
    COMPONENTS Interpreter
)

function(convert_pal_settings_name SETTINGS_FILE OUT_BASENAME_VAR)
    # Convert input name convention to output.
    # eg, settings_core.json -> g_coreSettings

    # 1. get basename
    get_filename_component(SETTINGS_FILE_BASENAME ${SETTINGS_FILE} NAME_WE)
    # 2. split on '_' into list
    string(REPLACE "_" ";" OUT_PARTS ${SETTINGS_FILE_BASENAME})
    # 3. reverse
    list(REVERSE OUT_PARTS)
    # 4. first part goes in unmodified
    list(POP_FRONT OUT_PARTS OUT_BASENAME)
    string(PREPEND OUT_BASENAME "g_")
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
    set(singleValArgs MAGIC_BUF CLASS_NAME CODE_TEMPLATE OUT_DIR OUT_BASENAME)
    set(multiValArgs SETTINGS ENSURE_DELETED)
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
            convert_pal_settings_name(${SETTINGS_FILE} OUT_BASENAME)
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

        # Note this doesn't track imported python libs/exe (mostly system deps).
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
        source_group("Generated/Settings" FILES
                     ${SETGEN_OUT_DIR}/${OUT_BASENAME}.h
                     ${SETGEN_OUT_DIR}/${OUT_BASENAME}.cpp
        )
    endforeach()
endfunction()

function(pal_setup_generated_code)
    set(COMMON_ARGS "")
    target_pal_settings(pal SETTINGS src/core/settings_core.json
                            ${COMMON_ARGS}
                            ENSURE_DELETED src/core/g_palSettings.h
                                           src/core/g_palSettings.cpp)
    target_pal_settings(pal SETTINGS src/core/settings_platform.json
                            ${COMMON_ARGS}
                            CODE_TEMPLATE ${PAL_GEN_DIR}/platformSettingsCodeTemplates.py
                            CLASS_NAME PlatformSettingsLoader
                            ENSURE_DELETED src/core/g_palPlatformSettings.h
                                           src/core/g_palPlatformSettings.cpp)
    if (PAL_BUILD_GFX6)
        target_pal_settings(pal SETTINGS src/core/hw/gfxip/gfx6/settings_gfx6.json
                                ${COMMON_ARGS}
                                ENSURE_DELETED src/core/hw/gfxip/gfx6/g_gfx6PalSettings.h
                                               src/core/hw/gfxip/gfx6/g_gfx6PalSettings.cpp)
    endif()
    if (PAL_BUILD_GFX9)
        target_pal_settings(pal SETTINGS src/core/hw/gfxip/gfx9/settings_gfx9.json
                                ${COMMON_ARGS}
                                ENSURE_DELETED src/core/hw/gfxip/gfx9/g_gfx9PalSettings.h
                                               src/core/hw/gfxip/gfx9/g_gfx9PalSettings.cpp)
    endif()

endfunction()

function(nongen_source_groups DIR TGT)
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
endfunction()
