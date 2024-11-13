add_executable(ut_server ut_server.cpp ${ALL_SRCS})
add_test(ut_server ut_server ut_server)
target_link_libraries(ut_server JResult)

add_executable(ut_client ut_client.cpp)
add_test(ut_client ut_client ut_client)
target_link_libraries(ut_client JResult)