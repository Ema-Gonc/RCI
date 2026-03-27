CC = gcc
CFLAGS = -std=c99 -pedantic -g -DDEBUG -Wall -Wextra -Werror -Wwrite-strings
OPTLVL = 3
TARGET = OWR

$(TARGET):
	$(CC) main.c src/*.c -o $(TARGET) -O$(OPTLVL) $(CFLAGS)

clean:
	rm -f ./$(TARGET)

tv:
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)