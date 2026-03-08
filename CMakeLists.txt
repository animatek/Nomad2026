cmake_minimum_required(VERSION 3.22)
project(Nomad2026 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_gui_app(Nomad2026
    PRODUCT_NAME "Nomad2026"
    COMPANY_NAME "Nomad2026"
    VERSION "0.1.0"
)

target_sources(Nomad2026 PRIVATE
    source/Main.cpp
    source/MainComponent.cpp

    # Data Model
    source/model/ModuleDescriptions.cpp
    source/model/Patch.cpp
    source/model/PatchParser.cpp
    source/model/PchFileIO.cpp
    source/model/ThemeData.cpp

    # UI
    source/ui/MainLayout.cpp
    source/ui/ModuleBrowserPanel.cpp
    source/ui/PatchCanvasComponent.cpp
    source/ui/InspectorPanel.cpp
    source/ui/StatusBar.cpp
    source/ui/PatchHeaderBar.cpp

    # UI
    source/ui/MidiSettingsDialog.cpp

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
    source/protocol/StorePatchMessage.cpp

    # Synchronization
    source/sync/PatchSynchronizer.cpp
)

target_include_directories(Nomad2026 PRIVATE source)

target_compile_definitions(Nomad2026 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Nomad2026,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Nomad2026,JUCE_VERSION>"
)

target_link_libraries(Nomad2026 PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
