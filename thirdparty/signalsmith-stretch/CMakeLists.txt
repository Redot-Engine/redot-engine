cmake_minimum_required(VERSION 3.24)

add_library(signalsmith-stretch INTERFACE)
set_target_properties(signalsmith-stretch PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include)

include(FetchContent)
FetchContent_Declare(
	signalsmith-linear
	GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
	GIT_TAG 0.2.6
	GIT_SHALLOW ON
)
FetchContent_MakeAvailable(signalsmith-linear)

target_link_libraries(signalsmith-stretch INTERFACE signalsmith-linear)
