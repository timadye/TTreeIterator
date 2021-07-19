cmake_minimum_required( VERSION 3.2 FATAL_ERROR )

project(TTreeIterator)

# =============================================================================
# General definitions
# =============================================================================

#set(CMAKE_CXX_STANDARD 11)   # should work with C++11 or later, but don't specify because this (-std=c++17 setting) needs to match the version ROOT was built with for string_view compatibility

if(APPLE)
    add_definitions(-DAPPLE)
endif()

add_custom_target(TTreeIterator_version
        COMMENT "Generating TTreeIteratorVersion.h ..."
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/versioning
        COMMAND ${CMAKE_COMMAND} -DOUTPUT_FILE=${CMAKE_BINARY_DIR}/versioning/temp_version.h
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/versioning.cmake
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/versioning/temp_version.h
        ${CMAKE_BINARY_DIR}/versioning/TTreeIteratorVersion.h
        COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/versioning/temp_version.h)

list(APPEND CMAKE_PREFIX_PATH $ENV{ROOTSYS})
find_package( ROOT COMPONENTS RIO Core Tree Hist ROOTTPython)
message(STATUS "Using ROOT From: ${ROOT_INCLUDE_DIRS}")
include(${ROOT_USE_FILE})

include_directories(${ROOT_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(${ROOT_CXX_FLAGS})

## get googletest
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        release-1.10.0
)
FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        main  # need main for benchmark::benchmark
)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()
FetchContent_GetProperties(googlebenchmark)
if(NOT googlebenchmark_POPULATED)
    FetchContent_Populate(googlebenchmark)
    add_subdirectory(${googlebenchmark_SOURCE_DIR} ${googlebenchmark_BINARY_DIR})
endif()

# =============================================================================
# TTreeIterator definitions
# =============================================================================

file(GLOB HEADERS TTreeIterator/*.h)

ROOT_GENERATE_DICTIONARY(G__TTreeIterator ${HEADERS} LINKDEF src/LinkDef.h)

file(GLOB SOURCES src/*.cxx)

add_library(TTreeIterator SHARED ${SOURCES} G__TTreeIterator )
target_link_libraries(TTreeIterator ${ROOT_LIBRARIES})

target_sources(TTreeIterator PRIVATE ${CMAKE_BINARY_DIR}/versioning/TTreeIteratorVersion.h)
set_source_files_properties(${CMAKE_BINARY_DIR}/versioning/TTreeIteratorVersion.h PROPERTIES GENERATED TRUE)
target_include_directories(TTreeIterator PRIVATE ${CMAKE_BINARY_DIR}/versioning)
target_include_directories(TTreeIterator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(TTreeIterator TTreeIterator_version)

set(SETUP ${CMAKE_CURRENT_BINARY_DIR}/setup.sh)
file(WRITE ${SETUP} "#!/bin/bash\n")
file(APPEND ${SETUP} "# this is an auto-generated setup script\n" )
file(APPEND ${SETUP} "export PYTHONPATH=\${PYTHONPATH}:${CMAKE_CURRENT_BINARY_DIR}\n")
file(APPEND ${SETUP} "export LD_LIBRARY_PATH=\${LD_LIBRARY_PATH}:${ROOT_LIBRARY_DIR}:${CMAKE_CURRENT_BINARY_DIR}\n")

enable_testing()
include(GoogleTest)
add_executable(TestIter test/iterTests.cxx)
add_executable(TestTiming test/timingTests.cxx)
add_executable(BenchAny test/anyBench.cxx)
target_link_libraries(TestIter TTreeIterator gtest gtest_main)
target_link_libraries(TestTiming TTreeIterator gtest gtest_main)
target_link_libraries(BenchAny TTreeIterator gtest benchmark::benchmark)

install( DIRECTORY TTreeIterator DESTINATION include FILES_MATCHING
        COMPONENT headers
        PATTERN "*.h"
        )

install( FILES ${CMAKE_CURRENT_BINARY_DIR}/libTTreeIterator.rootmap
        ${CMAKE_CURRENT_BINARY_DIR}/libTTreeIterator_rdict.pcm
        DESTINATION lib
        COMPONENT libraries)

install(TARGETS TTreeIterator
        LIBRARY DESTINATION lib
        COMPONENT libraries)

string(ASCII 27 Esc)
message(STATUS "${Esc}[1;35mIMPORTANT: Remember to run:
    source ${SETUP}${Esc}[m" )
