# Makefile for xattr-demo

CC=		cc
CFLAGS=		-Wall -Wextra

PROG=		xattr-demo
SRCS=		xattr-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG) test.txt

test: $(PROG)
	@echo "Testing xattr-demo..."
	@echo ""
	@echo "Creating test file..."
	@echo "CatBSD test file" > test.txt
	@echo ""
	@echo "=== Test 1: Write attributes ==="
	@./$(PROG) -w com.catbsd.tag "important" test.txt
	@./$(PROG) -w com.catbsd.author "CatBSD Team" test.txt
	@./$(PROG) -w com.catbsd.meow "🐱" test.txt
	@echo ""
	@echo "=== Test 2: List attributes ==="
	@./$(PROG) -l test.txt
	@echo ""
	@echo "=== Test 3: Read attributes ==="
	@echo -n "tag: "
	@./$(PROG) -p com.catbsd.tag test.txt
	@echo -n "author: "
	@./$(PROG) -p com.catbsd.author test.txt
	@echo -n "meow: "
	@./$(PROG) -p com.catbsd.meow test.txt
	@echo ""
	@echo "=== Test 4: Delete attribute ==="
	@./$(PROG) -d com.catbsd.author test.txt
	@./$(PROG) -l test.txt
	@echo ""
	@echo "=== Test 5: Clear all ==="
	@./$(PROG) -c test.txt
	@echo ""
	@echo "✓ Tests passed"
	@rm -f test.txt

.PHONY: all clean test
