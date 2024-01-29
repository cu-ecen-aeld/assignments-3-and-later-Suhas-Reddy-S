# Compiler and CFlAGS selection 'CROSS_COMPILE' is used to set custom arm compiler
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Werror

# Sets the source as writer.c and object as writer.o
SRC := writer.c
OBJ := $(SRC:.c=.o)

# Behaves as alias to object file
TARGET := writer

# This is executed by default and is redirected to compilation steps
all: $(TARGET)

# Looks for object and maps it to target
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Generates object when source is available
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# removes previously created objects and targets
clean:
	rm -f $(TARGET) $(OBJ)

# Doesn't execute if artifacts are already present
.PHONY: all clean
