CC = g++
CFLAGS = -g


TARGET = iosched

all: $(TARGET)
	@echo "Building ..."

$(TARGET): main.o
	$(CC) $(CFLAGS) -o $(TARGET) main.o

main.o: main.cpp
	$(CC) $(CFLAGS) -c main.cpp

clean:
	@echo "Cleaning up ..."
	rm -rf $(TARGET) main.o
