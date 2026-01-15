# Makefile for pbpaste-demo

CC=		cc
CFLAGS=		-Wall -Wextra

PROG=		pbpaste-demo
SRCS=		pbpaste-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG)

test: $(PROG)
	@echo "Testing pbpaste-demo..."
	@echo ""
	@echo "First, copying test data to clipboard..."
	@echo "CatBSD clipboard test!" | pbcopy
	@echo ""
	@echo "=== Test 1: Paste to stdout ==="
	@./$(PROG)
	@echo ""
	@echo ""
	@echo "=== Test 2: Paste to file ==="
	@./$(PROG) > test-output.txt
	@echo "Pasted to test-output.txt:"
	@cat test-output.txt
	@rm -f test-output.txt
	@echo ""
	@echo "✓ Tests passed"

.PHONY: all clean test
