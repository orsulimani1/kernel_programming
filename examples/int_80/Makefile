CC = gcc-11
CFLAGS = -m32 -Wall -Wextra -g

# Targets
TARGET = write_stdout_int80
SOURCE = write_stdout_int80.c

# Build the program
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Show assembly output
disasm: $(TARGET)
	objdump -d $(TARGET) | grep -A 20 "write_using_int80"

.PHONY: test clean install uninstall disasm debug