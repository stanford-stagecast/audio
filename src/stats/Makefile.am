AM_CPPFLAGS = -I$(srcdir)/../util $(GLU_CFLAGS) $(GLFW3_CFLAGS) $(GLEW_CFLAGS) $(CXX17_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

bin_PROGRAMS = client server

client_SOURCES = client.cc
client_LDADD = ../util/libstatsutil.a

server_SOURCES = server.cc
server_LDADD = ../util/libstatsutil.a
