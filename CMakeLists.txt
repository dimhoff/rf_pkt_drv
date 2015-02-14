project(si443x_drv)

cmake_minimum_required(VERSION 2.8.8)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(BUILD_TESTS "Build Unit Tests (Requires Check)" OFF)

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -Wall)

add_subdirectory(src)
if (BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif (BUILD_TESTS)
