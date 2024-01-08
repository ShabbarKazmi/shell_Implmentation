CC=gcc

CFLAGS = -c -Wall
LDFLAGS = -lm
DEPS = $(wildcard *.h)
SOURCES = $(wildcard *.c)
OBJ = $(SOURCES:.c=.o)
EXEC = Project2

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) $<

clean:
	rm -rf *o $(EXEC)