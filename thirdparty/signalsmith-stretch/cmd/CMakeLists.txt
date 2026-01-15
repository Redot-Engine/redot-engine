cmake_minimum_required(VERSION 3.24)
project(example-cmd VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 11)

add_executable(stretch ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

add_subdirectory(".." "build-stretch") # Needs a (relative) build directory since it's not a subdir
target_link_libraries(stretch signalsmith-stretch)

include(FetchContent)
set(FETCHCONTENT_QUIET False)
FetchContent_Declare(
	inputs
	URL https://signalsmith-audio.co.uk/code/stretch/inputs.zip
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/inputs"
)
FetchContent_MakeAvailable(inputs)

