enable_testing ()

add_test(NAME t_websocket_loop         COMMAND websocket-loop)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 10 -R '^t_')
