CC = g++
CFLAGS = -g


TARGET = sched

all: $(TARGET)
	@echo "Building ..."

$(TARGET): main.o $(TARGET).o
	$(CC) $(CFLAGS) -o $(TARGET) main.o $(TARGET).o

main.o: main.cpp
	$(CC) $(CFLAGS) -c main.cpp

$(TARGET).o: $(TARGET).cpp
	$(CC) $(CFLAGS) -c $(TARGET).cpp

clean:
	@echo "Cleaning up ..."
	rm -rf $(TARGET) $(TARGET).o main.o
