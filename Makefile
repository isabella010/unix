FILE1=./src/interfaceMonitor.cpp
FILE2=./src/networkMonitor.cpp

TARGET1=interfaceMon
TARGET2=networkMon

CC=g++

$(TARGET1): $(FILE1)
	$(CC) $(FILE1) -o $(TARGET1)

$(TARGET2): $(FILE2)
	$(CC) $(FILE2) -o $(TARGET2)

all: $(TARGET1) $(TARGET2)

clean:
	rm -f $(TARGET1) $(TARGET2)