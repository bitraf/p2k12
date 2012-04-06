CFLAGS = -Wall -g
LDFLAGS = -lreadline -lpq
OBJECTS = main.o queue.o postgresql.o

all: p2k12

p2k12: $(OBJECTS)
	$(CC) $(OBJECTS) $(OUTPUT_OPTION) $(CFLAGS) $(LDFLAGS)

install: p2k12
	install p2k12 -m 755 /usr/local/bin/
