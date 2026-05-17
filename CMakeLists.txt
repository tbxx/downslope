cmake_minimum_required(VERSION 3.22)
project(Downslope VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Downslope
    COMPANY_NAME "Mine"
    BUNDLE_ID com.mine.downslope
    PLUGIN_MANUFACTURER_CODE Mine
    PLUGIN_CODE Dnsl
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Downslope"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png"
)

juce_generate_juce_header(Downslope)

target_sources(Downslope PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/TapeStopEngine.cpp
)

target_include_directories(Downslope PRIVATE Source)

target_compile_definitions(Downslope PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Downslope PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Host application — minimal JUCE GUI app that loads/loops an audio file and
# runs it through Downslope using the system default audio output.
# -----------------------------------------------------------------------------

juce_add_gui_app(DownslopeHost
    PRODUCT_NAME "Downslope Host"
    COMPANY_NAME "Mine"
    BUNDLE_ID com.mine.downslopehost
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png"
)

juce_generate_juce_header(DownslopeHost)

target_sources(DownslopeHost PRIVATE
    Source/TapeStopEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    HostApp/Main.cpp
    HostApp/MainComponent.cpp
)

target_include_directories(DownslopeHost PRIVATE Source HostApp)

target_compile_definitions(DownslopeHost PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(DownslopeHost PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
