# Makefile for open-demo

CC=		cc
CFLAGS=		-Wall -Wextra

PROG=		open-demo
SRCS=		open-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG) test.txt

test: $(PROG)
	@echo "Testing open-demo..."
	@echo ""
	@echo "Creating test file..."
	@echo "CatBSD test" > test.txt
	@echo ""
	@echo "=== Test 1: Open file with default app ==="
	@./$(PROG) test.txt
	@sleep 1
	@echo ""
	@echo "=== Test 2: Open URL ==="
	@./$(PROG) https://github.com
	@echo ""
	@echo "✓ Tests passed"
	@rm -f test.txt

.PHONY: all clean test
