add_executable(si443x_drv main.c si443x.c parse_reg_file.c ring_buf.c sparse_buf.c dehexify.c spi.c)
add_dependencies(si443x_drv git_version)
