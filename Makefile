CC=gcc
CFLAGS=-Wall

OBJ=main.o node.o net.o message.o routing.o

OWR: $(OBJ)
	$(CC) $(CFLAGS) -o OWR $(OBJ)

clean:
	rm -f *.o OWR