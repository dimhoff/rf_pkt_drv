cmake_minimum_required(VERSION 2.8.8)
project(rf_pkt_drv C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

# Build Options
option(BUILD_TESTS "Build Unit Tests (Requires Check)" OFF)
set(DEFAULT_DEV_PATH "/dev/spidev0.0" CACHE STRING "Default SPI device path connected to the radio tranciever")
set(DEFAULT_SOCK_PATH "/tmp/rf_pkt.sock" CACHE STRING "Default client socket path")
set(DEFAULT_CFG_PATH "/etc/rf_pkt_regs.cfg" CACHE STRING "Default register configuration file path")
set(DEFAULT_IRQ_PIN -1 CACHE STRING "Default GPIO connected to IRQ pin of the module")

# TODO: make this a runtime option
set(RF_BACKEND "si443x" CACHE STRING "Radio Transceiver to use(ie. si443x or sx1231)")
set_property(CACHE RF_BACKEND PROPERTY STRINGS "si443x" "sx1231")

if (RF_BACKEND STREQUAL "si443x")
	set(RF_BACKEND_SI443x 1)
elseif (RF_BACKEND STREQUAL "sx1231")
	set(RF_BACKEND_SX1231 1)
else ()
	message(FATAL_ERROR "Invalid RF_BACKEND value")

endif ()
message(STATUS "Using RF Backend: ${RF_BACKEND}")

configure_file(config.h.in "${PROJECT_BINARY_DIR}/config.h")
include_directories("${PROJECT_BINARY_DIR}")

include(GitGetVersion)

add_subdirectory(src)
if (BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif (BUILD_TESTS)
