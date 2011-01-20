CC=gcc
CFLAGS=-g
TARGETS=sdriq-info
SRCS=sdriq.c sdriq.h
OBJS=sdriq.o

all: $(TARGETS)

$(TARGETS): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGETS)
