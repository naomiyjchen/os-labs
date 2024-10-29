CC = g++ 

# compiler flags
CFLAGS = -g

# The build target
TARGET = linker

all: $(TARGET)
	@echo "Building ..."
$(TARGET): $(TARGET).cpp
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).cpp
clean:
	@echo "Cleaning up ..."
	rm -f $(TARGET)
