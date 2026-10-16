cmake_minimum_required(VERSION 3.22)

project(MzDebirth VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# JUCE, and the machines' engines, live beside the repository rather than inside it: the
# engines are compiled straight from each synth's own repository, so a fix to one reaches
# both the synth and the rack. Override any of these with -D...=<path>.
#
set(JUCE_PATH  "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE"  CACHE PATH "Where JUCE is checked out")
set(MZ101_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Mz101" CACHE PATH "Where Mz101 is checked out")
set(MZ303_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Mz303" CACHE PATH "Where Mz303 is checked out")
set(MZ808_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Mz808" CACHE PATH "Where Mz808 is checked out")
set(MZ909_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Mz909" CACHE PATH "Where Mz909 is checked out")

# Mz950's repository may still be checked out under its old name.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Mz950")
    set(MZ950_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../Mz950")
else()
    set(MZ950_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../VirtualS950")
endif()
set(MZ950_PATH "${MZ950_DEFAULT}" CACHE PATH "Where Mz950 is checked out")

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR
        "No JUCE at ${JUCE_PATH}.\n"
        "  cd ${CMAKE_CURRENT_SOURCE_DIR}/..\n"
        "  git clone --depth 1 https://github.com/juce-framework/JUCE\n"
        "or pass -DJUCE_PATH=<wherever it is>.")
endif()

foreach(machine MZ101 MZ303 MZ808 MZ909)
    if(NOT EXISTS "${${machine}_PATH}/Source")
        message(FATAL_ERROR "No ${machine} at ${${machine}_PATH} - clone it beside this repository, "
                            "or pass -D${machine}_PATH=<wherever it is>.")
    endif()
endforeach()

# Mz950's engine is a level further down, in its plugin.
if(NOT EXISTS "${MZ950_PATH}/Plugin/Source/S950")
    message(FATAL_ERROR "No Mz950 at ${MZ950_PATH} - clone it beside this repository, "
                        "or pass -DMZ950_PATH=<wherever it is>.")
endif()

add_subdirectory("${JUCE_PATH}" JUCE)

#
# The engines, and the sequencer that plays them. None of it links JUCE.
#
# The machines' Source folders go on the include path after this one's, so the engines are
# named by their folder - "TB303/Engine.h" - and this project's own headers win any clash
# with the plugin files beside them there.
#
set(MZ950_ENGINE "${MZ950_PATH}/Plugin/Source/S950")

set(CORE_SOURCES
    Source/Core/Effects.cpp
    Source/Core/Grooves.cpp
    Source/Core/Key.cpp
    Source/Core/Rack.cpp
    Source/Core/Sampler.cpp
    "${MZ101_PATH}/Source/SH101/Engine.cpp"
    "${MZ303_PATH}/Source/TB303/Engine.cpp"
    "${MZ808_PATH}/Source/TR808/Engine.cpp"
    "${MZ909_PATH}/Source/TR909/Engine.cpp"
    "${MZ909_PATH}/Source/TR909/Roms.cpp"
    "${MZ950_ENGINE}/Engine.cpp"
    "${MZ950_ENGINE}/Voice.cpp"
    "${MZ950_ENGINE}/Disk.cpp"
    "${MZ950_ENGINE}/Hfe.cpp")

set(CORE_INCLUDES
    Source
    "${MZ101_PATH}/Source"
    "${MZ303_PATH}/Source"
    "${MZ808_PATH}/Source"
    "${MZ909_PATH}/Source"
    "${MZ950_PATH}/Plugin/Source")

# The sample disks that come with Mz950, for the rack to open with and the check to play.
set(CORE_DEFINITIONS "MZ950_DISKS=\"${MZ950_PATH}/disks\"")

#
# A standalone app for now. It is built as a plugin with only the Standalone format, which
# brings JUCE's audio device settings with it, and leaves a VST3 one line away.
#
# The two codes are the plugin's IDENTITY and must never change once anyone has saved a
# project with it.
#
juce_add_plugin(MzDebirth
    COMPANY_NAME                "simozzer"
    COMPANY_COPYRIGHT           "Copyright (C) 2026 Simon Moscrop"
    PRODUCT_NAME                "MzDebirth"
    PLUGIN_MANUFACTURER_CODE    Smoz          # never change - see above
    PLUGIN_CODE                 Dbr1          # never change - see above

    VERSION                     "${PROJECT_VERSION}"

    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE

    FORMATS                     Standalone)

target_sources(MzDebirth PRIVATE
    Source/Processor.cpp
    Source/UI/Look.cpp
    Source/UI/Widgets.cpp
    Source/UI/MonoRack.cpp
    Source/UI/DrumRack.cpp
    Source/UI/EffectsBar.cpp
    Source/UI/KeyPanel.cpp
    Source/UI/SamplerRack.cpp
    Source/UI/MainPanel.cpp
    ${CORE_SOURCES})

target_include_directories(MzDebirth PRIVATE ${CORE_INCLUDES})
target_compile_definitions(MzDebirth PRIVATE ${CORE_DEFINITIONS})

target_compile_definitions(MzDebirth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MzDebirth
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#
# The sequencer's own check, held to timing, slides, pattern changes and level before an
# app is built round it.
#
add_executable(SeqCheck Tests/SeqCheck.cpp ${CORE_SOURCES})
target_include_directories(SeqCheck PRIVATE ${CORE_INCLUDES})
target_compile_definitions(SeqCheck PRIVATE ${CORE_DEFINITIONS})

enable_testing()
add_test(NAME sequencer COMMAND SeqCheck)
