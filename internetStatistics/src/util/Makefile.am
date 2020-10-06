AM_CPPFLAGS = $(CXX17_FLAGS) $(SSL_CFLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS)

noinst_LIBRARIES = libmushutil.a

libmushutil_a_SOURCES = exception.hh \
	file_descriptor.hh file_descriptor.cc \
	simple_string_span.hh \
	address.hh address.cc \
	eventloop.hh eventloop.cc \
	socket.hh socket.cc \
	ring_buffer.hh ring_buffer.cc \
	timer.hh timer.cc
