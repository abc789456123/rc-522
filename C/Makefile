CC=gcc
CFLAGS=-Wall -Wextra -O2

TARGET=rc522_test
SRCS=main.c rc522.c

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)