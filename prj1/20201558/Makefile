CC = gcc
TARGET = testlib
OBJS =  main.o bitmap.o debug.o hash.o hex_dump.o list.o
HEADER = bitmap.h debug.h hash.h hex_dump.h limits.h list.h round.h
all : $(TARGET)

$(TARGET) : $(OBJS) $(HEADER)
	$(CC) -o $(TARGET) $(OBJS)

clean : 
	rm $(OBJS)
	rm $(TARGET)