include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

# If pkg-config is not installed on the system, then the
# CHECK_INSTALL_DIR variable must be set to the install
# location of Check. For example, on Windows, this may
# be: C:/Program Files/check
# set(CHECK_INSTALL_DIR "C:/Program Files/check")

find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})

add_executable(check_ring_buf test_ring_buf.c ${PROJECT_SOURCE_DIR}/src/ring_buf.c)
target_link_libraries(check_ring_buf ${CHECK_LIBRARIES} -pthread)

add_executable(check_sparse_buf test_sparse_buf.c ${PROJECT_SOURCE_DIR}/src/sparse_buf.c)
target_link_libraries(check_sparse_buf ${CHECK_LIBRARIES} -pthread)

add_executable(check_dehexify test_dehexify.c ${PROJECT_SOURCE_DIR}/src/dehexify.c)
target_link_libraries(check_dehexify ${CHECK_LIBRARIES} -pthread)

add_executable(check_parse_reg_file
	test_parse_reg_file.c
	recursive_rmdir.c
	${PROJECT_SOURCE_DIR}/src/parse_reg_file.c
	${PROJECT_SOURCE_DIR}/src/sparse_buf.c
	${PROJECT_SOURCE_DIR}/src/dehexify.c
)
target_link_libraries(check_parse_reg_file ${CHECK_LIBRARIES} -pthread)

add_test(NAME check_ring_buf COMMAND check_ring_buf)
add_test(NAME check_sparse_buf COMMAND check_sparse_buf)
add_test(NAME check_dehexify COMMAND check_dehexify)
add_test(NAME check_parse_reg_file COMMAND check_parse_reg_file)
