SUBDIRS = src

.PHONY: format
format:
	find $(srcdir) -name '*.cc' -o -name '*.hh' | xargs clang-format -i
