cmake_minimum_required(VERSION 3.22)

project(MzDebirth VERSION 0.15.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# JUCE, the machines' engines and the PCF live beside the repository rather than inside it:
# each engine is compiled straight from its own repository - the synths', and MzPCF's - so a
# fix to one reaches both its plugin and the rack. Override any of these with -D...=<path>.
#
set(JUCE_PATH  "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE"  CACHE PATH "Where JUCE is checked out")
set(MZ101_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Mz101" CACHE PATH "Where Mz101 is checked out")
set(MZ303_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Mz303" CACHE PATH "Where Mz303 is checked out")
set(MZ808_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Mz808" CACHE PATH "Where Mz808 is checked out")
set(MZ909_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Mz909" CACHE PATH "Where Mz909 is checked out")
set(MZPCF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../MzPCF" CACHE PATH "Where MzPCF is checked out")
set(MZ7_PATH   "${CMAKE_CURRENT_SOURCE_DIR}/../Mz7"   CACHE PATH "Where Mz7 is checked out")
set(MZ106_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Mz106" CACHE PATH "Where Mz106 is checked out")

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

foreach(machine MZ101 MZ303 MZ808 MZ909 MZ7 MZ106 MZPCF)
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
# The engines, the PCF, and the sequencer that plays them. None of it links JUCE.
#
# The machines' Source folders go on the include path after this one's, so the engines are
# named by their folder - "TB303/Engine.h" - and this project's own headers win any clash
# with the plugin files beside them there.
#
set(MZ950_ENGINE "${MZ950_PATH}/Plugin/Source/S950")

set(CORE_SOURCES
    Source/Core/Bounce.cpp
    Source/Core/DemoSong.cpp
    Source/Core/Effects.cpp
    Source/Core/Fm.cpp
    Source/Core/Juno.cpp
    Source/Core/Grooves.cpp
    Source/Core/Key.cpp
    Source/Core/Rack.cpp
    Source/Core/Recorder.cpp
    Source/Core/Sampler.cpp
    Source/Core/Song.cpp
    "${MZ101_PATH}/Source/SH101/Engine.cpp"
    "${MZ303_PATH}/Source/TB303/Engine.cpp"
    "${MZ808_PATH}/Source/TR808/Engine.cpp"
    "${MZ909_PATH}/Source/TR909/Engine.cpp"
    "${MZ909_PATH}/Source/TR909/Roms.cpp"
    "${MZ950_ENGINE}/Engine.cpp"
    "${MZ950_ENGINE}/Voice.cpp"
    "${MZ950_ENGINE}/Disk.cpp"
    "${MZ950_ENGINE}/Hfe.cpp"
    "${MZ7_PATH}/Source/DX7/Engine.cpp"
    "${MZ7_PATH}/Source/DX7/Patch.cpp"
    "${MZ7_PATH}/Source/DX7/Banks.cpp"
    "${MZ106_PATH}/Source/J106/Engine.cpp"
    "${MZ106_PATH}/Source/J106/Patch.cpp"
    "${MZ106_PATH}/Source/J106/Banks.cpp"
    "${MZPCF_PATH}/Source/PCF/Pcf.cpp")

set(CORE_INCLUDES
    Source
    "${MZ101_PATH}/Source"
    "${MZ303_PATH}/Source"
    "${MZ808_PATH}/Source"
    "${MZ909_PATH}/Source"
    "${MZ950_PATH}/Plugin/Source"
    "${MZ7_PATH}/Source"
    "${MZ106_PATH}/Source"
    "${MZPCF_PATH}/Source")

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
    COMPANY_COPYRIGHT           "Created by Simon Moscrop"
    PRODUCT_NAME                "MzDebirth"
    PLUGIN_MANUFACTURER_CODE    Smoz          # never change - see above
    PLUGIN_CODE                 Dbr1          # never change - see above

    VERSION                     "${PROJECT_VERSION}"

    # the app's icon, made from one picture into the Windows .ico and the Mac .icns -
    # drawn by Resources/Icon.cs; Resources/icon-256.png is the Linux menu's copy
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"

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
    Source/UI/FmRack.cpp
    Source/UI/JunoRack.cpp
    Source/UI/MainPanel.cpp
    Source/UI/Arrange/ArrangeState.cpp
    Source/UI/Arrange/ArrangeTransport.cpp
    Source/UI/Arrange/ArrangeView.cpp
    Source/UI/Arrange/PartEditor.cpp
    Source/UI/Arrange/PartLane.cpp
    Source/UI/Arrange/Ruler.cpp
    Source/UI/Arrange/TrackList.cpp
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

# A probe for listening questions put into numbers - not a check, and not run by ctest.
add_executable(KickProbe Tests/KickProbe.cpp ${CORE_SOURCES})
target_include_directories(KickProbe PRIVATE ${CORE_INCLUDES})
target_compile_definitions(KickProbe PRIVATE ${CORE_DEFINITIONS})

# Both make whole racks as locals, and a rack - six machines' patterns, the Mz7's sixteen
# voices, the recording ring - is a good fraction of Windows' 1 MB default stack. Linux and
# macOS give a program 8 MB; this gives it 16 on Windows too. The app keeps its rack on the heap.
if(MSVC)
    target_link_options(SeqCheck PRIVATE /STACK:16777216)
    target_link_options(KickProbe PRIVATE /STACK:16777216)
endif()

enable_testing()
add_test(NAME sequencer COMMAND SeqCheck)
