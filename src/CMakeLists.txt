if (RF_BACKEND_SX1231)
	set(DEVICE_SOURCES sx1231.c)
else (RF_BACKEND_SX1231)
	set(DEVICE_SOURCES si443x.c)
endif (RF_BACKEND_SX1231)

add_executable(rf_pkt_drv main.c ${DEVICE_SOURCES} parse_reg_file.c ring_buf.c sparse_buf.c dehexify.c spi.c)
add_dependencies(rf_pkt_drv git_version)
