CC=gcc
CFLAGS=-I. -std=c++1z 
OBJ = client.o #address.o eventloop.o file_descriptor.o socket.o timer.o

%.o: %.cc $(OBJ)
	$(CC) -c -o $@ $< $(CFLAGS)

client: $(OBJ) $(LIB)
	$(CC) -o $@ $^ $(CFLAGS)
