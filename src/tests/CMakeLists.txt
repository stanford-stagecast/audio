add_executable (websocket-loop "websocket-loop.cc")
target_link_libraries ("websocket-loop" http)
target_link_libraries ("websocket-loop" util)
