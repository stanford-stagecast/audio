TARGET_CPPS := client.cc server.cc
CPP_FILES := $(filter-out $(TARGET_CPPS),$(wildcard *.cc))
OBJ_FILES := $(CPP_FILES:.cc=.o)
H_FILES   := $(wildcard *.hh)

CPP_FLAGS = --std=c++17 -Wall -Werror -Wpedantic -Ofast -g

all: client server

client: $(OBJ_FILES) client.o
		g++ -o $@ $^

server: $(OBJ_FILES) server.o
		g++ -o $@ $^

%.o: %.cc $(H_FILES) Makefile
		g++ -c $(CPP_FLAGS) -o $@ $<

.PHONY: clean

clean:
		rm -f *.o client server *~