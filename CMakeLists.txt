cmake_minimum_required(VERSION 3.22)
project(SubdivFX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# En macOS: binario universal (Apple Silicon + Intel)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# JUCE: carpeta ./JUCE (git clone https://github.com/juce-framework/JUCE.git)
add_subdirectory(JUCE)

option(SUBDIV_COPY_AFTER_BUILD "Copiar el plugin a la carpeta de plugins del sistema" ON)

set(SUBDIV_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SUBDIV_FORMATS AU)
endif()

juce_add_plugin(SubdivFX
    COMPANY_NAME            "0600musica"
    BUNDLE_ID               "com.0600musica.subdivfx"
    PLUGIN_MANUFACTURER_CODE O6mu
    PLUGIN_CODE             Sbdv
    PRODUCT_NAME            "SubdivFX"
    FORMATS                 ${SUBDIV_FORMATS}

    # Es un efecto MIDI puro (en Logic aparece en "MIDI FX")
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    IS_MIDI_EFFECT          TRUE
    AU_MAIN_TYPE            kAudioUnitType_MIDIProcessor
    VST3_CATEGORIES         Fx

    COPY_PLUGIN_AFTER_BUILD ${SUBDIV_COPY_AFTER_BUILD}
)

target_sources(SubdivFX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PianoRoll.cpp)

target_compile_definitions(SubdivFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(SubdivFX
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
