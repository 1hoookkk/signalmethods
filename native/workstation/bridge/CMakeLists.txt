find_package(Matlab REQUIRED COMPONENTS MX_LIBRARY MAIN_PROGRAM)
include(FetchContent)
FetchContent_Declare(miniaudio
  URL https://codeload.github.com/mackron/miniaudio/tar.gz/f40cf03f80cdb7e741d43e53b7e706e8c1394bcf
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(miniaudio)
set(model "${CMAKE_CURRENT_SOURCE_DIR}/../Source/model")
matlab_add_mex(NAME trench_bridge SRC trench_bridge.cpp bridge_values.cpp bridge_library.cpp bridge_commands.cpp bridge_laws.cpp
  "${model}/Frame.cpp" "${model}/Morph.cpp" "${model}/Body.cpp"
  "${model}/Library.cpp" "${model}/Delaunay.cpp" "${model}/Stitch.cpp"
  LINK_TO trench_native_core juce::juce_core juce::juce_graphics juce::juce_recommended_config_flags)
matlab_add_mex(NAME trench_audio SRC trench_audio.cpp audio_engine.cpp
  LINK_TO trench_native_core)
target_include_directories(trench_bridge PRIVATE "${model}")
target_include_directories(trench_audio PRIVATE "${miniaudio_SOURCE_DIR}")
target_compile_definitions(trench_bridge PRIVATE JUCE_STANDALONE_APPLICATION=1
  JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
foreach(target trench_bridge trench_audio)
  target_compile_features(${target} PRIVATE cxx_std_20)
  set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/../toolbox/mex"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target}>" "${CMAKE_CURRENT_SOURCE_DIR}/../toolbox/mex/")
endforeach()
foreach(suite bridge headspace shot envelope)
  string(SUBSTRING "${suite}" 0 1 initial)
  string(TOUPPER "${initial}" initial)
  string(SUBSTRING "${suite}" 1 -1 rest)
  add_test(NAME trench_workstation_${suite}
    COMMAND "${Matlab_MAIN_PROGRAM}" -batch "addpath('toolbox'); trench.setup; r=runtests('tests/t${initial}${rest}.m'); assertSuccess(r);"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
  set_tests_properties(trench_workstation_${suite} PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
endforeach()
