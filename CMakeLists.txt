cmake_minimum_required(VERSION 3.22)
project(Nomad2026 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)
option(NOMAD_BUILD_PLUGIN "Build VST3/CLAP plugin targets" OFF)

if(NOMAD_BUILD_PLUGIN)
    set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(libs/clap-juce-extensions)
endif()

# --- Shared source files (used by both standalone and plugin) ---
set(NOMAD_SHARED_SOURCES
    source/MainComponent.cpp

    # Data Model
    source/model/ModuleDescriptions.cpp
    source/model/Patch.cpp
    source/model/PatchParser.cpp
    source/model/PatchSerializer.cpp
    source/model/PchFileIO.cpp
    source/model/ThemeData.cpp

    # UI
    source/ui/MainLayout.cpp
    source/ui/ModuleBrowserPanel.cpp
    source/ui/PatchCanvasComponent.cpp
    source/ui/InspectorPanel.cpp
    source/ui/StatusBar.cpp
    source/ui/PatchHeaderBar.cpp
    source/ui/PatchLocationDialog.cpp
    source/ui/PatchSettingsDialog.cpp
    source/ui/MidiSettingsDialog.cpp
    source/ui/QuickAddPopup.cpp
    source/ui/PatchBrowserPanel.cpp

    # MIDI/SysEx
    source/midi/SysExCodec.cpp
    source/midi/NmMessages.cpp
    source/midi/NmProtocol.cpp
    source/midi/MidiDeviceManager.cpp
    source/midi/ConnectionManager.cpp

    # Protocol Messages
    source/protocol/SysExMessage.cpp
    source/protocol/NewCableMessage.cpp
    source/protocol/DeleteCableMessage.cpp
    source/protocol/MoveModuleMessage.cpp
    source/protocol/NewModuleMessage.cpp
    source/protocol/DeleteModuleMessage.cpp
    source/protocol/ParameterEncoder.cpp
    source/protocol/StorePatchMessage.cpp
    source/protocol/SetPatchTitleMessage.cpp
    source/protocol/MorphAssignmentMessage.cpp
    source/protocol/MorphKeyboardAssignmentMessage.cpp
    source/protocol/MorphRangeChangeMessage.cpp
    source/protocol/KnobAssignmentMessage.cpp
    source/protocol/MidiCtrlAssignmentMessage.cpp

    # Synchronization
    source/sync/PatchSynchronizer.cpp

    # Help system
    source/help/ModuleHelpData.cpp
    source/help/ModuleHelpPopup.cpp
)

# --- Standalone Application ---
juce_add_gui_app(Nomad2026
    PRODUCT_NAME "Nomad2026"
    COMPANY_NAME "Nomad2026"
    VERSION "0.1.0"
)

target_sources(Nomad2026 PRIVATE
    source/Main.cpp
    ${NOMAD_SHARED_SOURCES}
)

if(NOMAD_BUILD_PLUGIN)
    # --- VST3/AU Plugin ---
    juce_add_plugin(Nomad2026Plugin
        PLUGIN_MANUFACTURER_CODE Nmad
        PLUGIN_CODE Nm26
        FORMATS VST3 AU Standalone
        PRODUCT_NAME "Nomad2026"
        COMPANY_NAME "Nomad2026"
        IS_MIDI_EFFECT TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        VERSION "0.1.0"
    )

    target_sources(Nomad2026Plugin PRIVATE
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginEditor.cpp
        ${NOMAD_SHARED_SOURCES}
    )

    clap_juce_extensions_plugin(
        TARGET Nomad2026Plugin
        CLAP_ID "com.nomad2026.editor"
        CLAP_FEATURES "note-effect" "utility"
        CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 0
    )
endif()

juce_add_binary_data(NomadData SOURCES
    data/modules.xml
    data/transformations.xml
    data/classic-theme.xml
)

# --- Shared settings ---
set(NOMAD_TARGETS Nomad2026)
if(NOMAD_BUILD_PLUGIN)
    list(APPEND NOMAD_TARGETS Nomad2026Plugin)
endif()

foreach(target ${NOMAD_TARGETS})
    target_include_directories(${target} PRIVATE source)

    target_compile_definitions(${target} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="Nomad2026"
        JUCE_APPLICATION_VERSION_STRING="0.1.0"
        JUCE_VST3_CAN_REPLACE_VST2=0
    )

    target_link_libraries(${target} PRIVATE
        NomadData
        juce::juce_core
        juce::juce_events
        juce::juce_data_structures
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
endforeach()
