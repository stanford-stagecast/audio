add_executable (sender "sender.cc")
target_link_libraries ("sender" util)

add_executable (receiver "receiver.cc")
target_link_libraries ("receiver" util)

add_executable (server "server.cc")
target_link_libraries ("server" util)
