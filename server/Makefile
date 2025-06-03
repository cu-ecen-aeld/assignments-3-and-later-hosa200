CC ?= ${CROSS_COMPILE}gcc
CFLAGS ?= -g -Wall -Werror
TARGET ?= aesdsocket
OBJ = aesdsocket.o
LDFLAGS ?= -lpthread

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o