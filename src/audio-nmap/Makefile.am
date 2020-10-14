AM_CPPFLAGS = $(CXX17_FLAGS) $(fftw3f_CFLAGS) $(alsa_CFLAGS) $(sndfile_CFLAGS)
AM_CXXFLAGS = $(PICKY_CXXFLAGS)

LDFLAGS = -pthread
LDADD = -lfftw3f_threads $(fftw3f_LIBS) $(alsa_LIBS) $(sndfile_LIBS) -lm

SHARED = helpers.hh fft.hh fft.cc audio.hh audio.cc wav.hh wav.cc

bin_PROGRAMS = chazonance

chazonance_SOURCES = chazonance.cc $(SHARED)
