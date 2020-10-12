AM_CPPFLAGS = $(CXX11_FLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS) $(NODEBUG_CXXFLAGS)

noinst_LIBRARIES = libstatsutil.a

libstatsutil_a_SOURCES = address.hh address.cc eventloop.hh eventloop.cc \
	exception.hh file_descriptor.hh filedescriptor.cc simple_string_span.hh \
	socket.hh socket.cc timer.hh timer.cc ring_buffer.hh ring_buffer.cc \
