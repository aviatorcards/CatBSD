# Makefile for say-demo

CC=		cc
CFLAGS=		-Wall -Wextra

PROG=		say-demo
SRCS=		say-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG)

test: $(PROG)
	@echo "Testing say-demo..."
	@./$(PROG) "CatBSD is purring to life!"
	@sleep 1
	@./$(PROG) -v Alex "Hello from FreeBSD with Darwin compatibility"
	@sleep 1
	@./$(PROG) --voice-list
	@echo "✓ Tests passed"

.PHONY: all clean test
