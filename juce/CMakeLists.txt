cmake_minimum_required(VERSION 3.22)
project(QuietMachineNative VERSION 0.2.0 LANGUAGES C CXX)

option(QM_BUILD_SMOKE_TESTS "Build processor and editor verification" ON)
if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to a JUCE 8 checkout")
endif()
add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(QuietMachineVX
    COMPANY_NAME "Quiet Machine Audio"
    BUNDLE_ID "audio.quietmachine.native"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Qmch
    PLUGIN_CODE Qmnt
    FORMATS VST3 Standalone
    PRODUCT_NAME "Quiet Machine Native")

target_sources(QuietMachineVX PRIVATE
    Source/NativeAmbientEngine.cpp Source/NativeAmbientEngine.h
    Source/NativeMasterFx.cpp Source/NativeMasterFx.h
    Source/NativeTextureEngine.cpp Source/NativeTextureEngine.h
    Source/SourceOutput.h
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h)
target_compile_features(QuietMachineVX PRIVATE cxx_std_20)
target_compile_definitions(QuietMachineVX PUBLIC
    QM_ENABLE_WEBVIEW2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)
target_link_libraries(QuietMachineVX PRIVATE
    juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
juce_generate_juce_header(QuietMachineVX)

if(QM_BUILD_SMOKE_TESTS)
    juce_add_console_app(QuietMachineSmoke PRODUCT_NAME "Quiet Machine Native Verification")
    target_sources(QuietMachineSmoke PRIVATE Tests/ProcessorSmokeTest.cpp)
    target_compile_features(QuietMachineSmoke PRIVATE cxx_std_20)
    target_include_directories(QuietMachineSmoke PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/QuietMachineVX_artefacts/JuceLibraryCode)
    target_compile_definitions(QuietMachineSmoke PRIVATE JucePlugin_Name="Quiet Machine Native")
    target_link_libraries(QuietMachineSmoke PRIVATE QuietMachineVX juce::juce_audio_utils juce::juce_dsp)
endif()
