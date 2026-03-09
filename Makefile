CC=gcc
CFLAGS=-Wall

OBJ=main.o node.o net.o message.o routing.o

overlay: $(OBJ)
	$(CC) $(CFLAGS) -o overlay $(OBJ)

clean:
	rm -f *.o overlay