# Makefile for pbcopy-demo

CC=		cc
CFLAGS=		-Wall -Wextra

PROG=		pbcopy-demo
SRCS=		pbcopy-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG)

test: $(PROG)
	@echo "Testing pbcopy-demo..."
	@echo ""
	@echo "=== Test 1: Copy text ==="
	@echo "Hello from CatBSD!" | ./$(PROG)
	@echo ""
	@echo "=== Test 2: Copy file ==="
	@echo "This is a test file" > test.txt
	@./$(PROG) < test.txt
	@rm -f test.txt
	@echo ""
	@echo "✓ Tests passed"
	@echo "Check your clipboard - it should contain the test data!"

.PHONY: all clean test
