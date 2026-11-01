# mm_add_plugin(NAME <id> PRODUCT_NAME "<Name>" CODE <4 chars>)
#   - runs tools/gen_params.py on plugins/<id>/params.json -> build/generated/<id>/{Params.h,tool_schema.json}
#   - creates the JUCE plugin target <id> in VST3/CLAP/Standalone (+AU on macOS, +AAX when MM_AAX_SDK_PATH is set)
#   - compiles tool_schema.json and presets/*.mmpreset into <id>_data (BinaryData)
function(mm_add_plugin)
    cmake_parse_arguments(MM "" "NAME;PRODUCT_NAME;CODE" "" ${ARGN})
    if(NOT MM_NAME OR NOT MM_PRODUCT_NAME OR NOT MM_CODE)
        message(FATAL_ERROR "mm_add_plugin needs NAME, PRODUCT_NAME and CODE")
    endif()

    set(gen_dir ${CMAKE_BINARY_DIR}/generated/${MM_NAME})
    set(params_json ${CMAKE_CURRENT_SOURCE_DIR}/params.json)
    set(generator ${CMAKE_SOURCE_DIR}/tools/gen_params.py)
    file(MAKE_DIRECTORY ${gen_dir})
    # Generate at configure time (so juce_add_binary_data can see the schema file) and again at build time on change.
    execute_process(COMMAND ${MM_PYTHON} ${generator} ${params_json} --out ${gen_dir}
                    RESULT_VARIABLE gen_result OUTPUT_VARIABLE gen_out ERROR_VARIABLE gen_err)
    if(NOT gen_result EQUAL 0)
        message(FATAL_ERROR "params.json invalid for ${MM_NAME}:\n${gen_out}${gen_err}")
    endif()
    add_custom_command(OUTPUT ${gen_dir}/Params.h ${gen_dir}/tool_schema.json
        COMMAND ${MM_PYTHON} ${generator} ${params_json} --out ${gen_dir}
        DEPENDS ${params_json} ${generator}
        COMMENT "Generating parameters for ${MM_NAME}")
    add_custom_target(${MM_NAME}_params DEPENDS ${gen_dir}/Params.h ${gen_dir}/tool_schema.json)

    set(formats VST3 CLAP Standalone)
    if(APPLE)
        list(APPEND formats AU)
    endif()
    if(MM_AAX_SDK_PATH)
        list(APPEND formats AAX)
    else()
        message(STATUS "AAX stubbed for ${MM_NAME}: MM_AAX_SDK_PATH not set")
    endif()

    juce_add_plugin(${MM_NAME}
        PRODUCT_NAME "${MM_PRODUCT_NAME}"
        COMPANY_NAME "MixMaster"
        BUNDLE_ID "com.mixmaster.${MM_NAME}"
        PLUGIN_MANUFACTURER_CODE Mxms
        PLUGIN_CODE ${MM_CODE}
        FORMATS ${formats}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx
        AU_MAIN_TYPE kAudioUnitType_Effect)

    clap_juce_extensions_plugin(TARGET ${MM_NAME}
        CLAP_ID "com.mixmaster.${MM_NAME}"
        CLAP_FEATURES audio-effect)

    file(GLOB presets CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/presets/*.mmpreset)
    juce_add_binary_data(${MM_NAME}_data SOURCES ${gen_dir}/tool_schema.json ${presets})
    add_dependencies(${MM_NAME}_data ${MM_NAME}_params)
    add_dependencies(${MM_NAME} ${MM_NAME}_params)

    target_include_directories(${MM_NAME} PRIVATE ${gen_dir} ${CMAKE_SOURCE_DIR})
    target_compile_definitions(${MM_NAME} PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=0)
    target_link_libraries(${MM_NAME}
        PRIVATE mm_core mm_plugin_base mm_gui ${MM_NAME}_data juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
    mm_set_warnings(${MM_NAME})
    mm_enable_sanitizers(${MM_NAME})
endfunction()
