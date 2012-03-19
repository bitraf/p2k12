CFLAGS = -Wall -g
LDFLAGS = -lreadline
OBJECTS = main.o queue.o

all: p2k12

p2k12: $(OBJECTS)
	$(CC) $(OBJECTS) $(OUTPUT_OPTION) $(CFLAGS) $(LDFLAGS)
