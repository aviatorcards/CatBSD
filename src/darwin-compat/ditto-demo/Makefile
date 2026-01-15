# Makefile for ditto-demo

CC=		cc
CFLAGS=		-Wall -Wextra

PROG=		ditto-demo
SRCS=		ditto-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG)
	rm -rf test-src test-dst test-archive.zip test-extracted

test: $(PROG)
	@echo "Testing ditto-demo..."
	@echo ""
	@echo "=== Test 1: Copy file ==="
	@echo "CatBSD test" > test-file.txt
	@./$(PROG) test-file.txt test-copy.txt
	@cat test-copy.txt
	@echo ""
	@echo "=== Test 2: Copy directory ==="
	@mkdir -p test-src
	@echo "File 1" > test-src/file1.txt
	@echo "File 2" > test-src/file2.txt
	@./$(PROG) test-src test-dst
	@ls -la test-dst/
	@echo ""
	@echo "=== Test 3: Create archive ==="
	@./$(PROG) -c -k test-src test-archive.zip
	@ls -lh test-archive.zip
	@echo ""
	@echo "=== Test 4: Extract archive ==="
	@./$(PROG) -x -k test-archive.zip test-extracted
	@ls -la test-extracted/
	@echo ""
	@echo "✓ Tests passed"
	@rm -rf test-file.txt test-copy.txt test-src test-dst test-archive.zip test-extracted

.PHONY: all clean test
