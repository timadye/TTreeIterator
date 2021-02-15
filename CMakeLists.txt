cmake_minimum_required( VERSION 3.2 FATAL_ERROR )
set(CMAKE_CXX_STANDARD 17)
project(TTreeIterator)

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
find_package( ROOT COMPONENTS MathCore RIO Core Tree Hist TreePlayer RooFit RooFitCore Matrix Gpad Graf RooStats HistPainter HistFactory )
message(STATUS "Using ROOT From: ${ROOT_INCLUDE_DIRS}")
include(${ROOT_USE_FILE})

include_directories(${ROOT_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(${ROOT_CXX_FLAGS})

## get googletest
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        release-1.8.0
)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()


add_subdirectory(TTreeIterator)
#add_subdirectory(Tools)