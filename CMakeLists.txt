cmake_minimum_required(VERSION 2.8.8)
project(si443x_drv C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

# Build Options
option(BUILD_TESTS "Build Unit Tests (Requires Check)" OFF)
set(DEFAULT_DEV_PATH "/dev/spidev0.0" CACHE STRING "Default SPI device path connected to Si443x")
set(DEFAULT_SOCK_PATH "/tmp/si443x.sock" CACHE STRING "Default client socket path")
set(DEFAULT_CFG_PATH "/etc/si443x_regs.cfg" CACHE STRING "Default register configuration file path")

configure_file(config.h.in "${PROJECT_BINARY_DIR}/config.h")
include_directories("${PROJECT_BINARY_DIR}")

include(GitGetVersion)

add_subdirectory(src)
if (BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif (BUILD_TESTS)
