# Makefile for defaults-demo

CC=		cc
CFLAGS=		-Wall -Wextra

# Platform-specific libxml2 paths
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -I/opt/homebrew/opt/libxml2/include/libxml2
    LDFLAGS = -L/opt/homebrew/opt/libxml2/lib -lxml2
else ifeq ($(UNAME_S),FreeBSD)
    CFLAGS += -I/usr/local/include/libxml2
    LDFLAGS = -L/usr/local/lib -lxml2
endif
LIBS=		-lxml2

PROG=		defaults-demo
SRCS=		defaults-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG)
	rm -rf ~/Library/Preferences/com.catbsd.*.plist

test: $(PROG)
	@echo "Testing defaults-demo..."
	@echo ""
	@echo "=== Test 1: Write preferences ==="
	@./$(PROG) write com.catbsd.test theme dark
	@./$(PROG) write com.catbsd.test font "Monaco 12"
	@./$(PROG) write com.catbsd.test meow "🐱"
	@echo ""
	@echo "=== Test 2: Read preferences ==="
	@echo -n "theme: "
	@./$(PROG) read com.catbsd.test theme
	@echo -n "font: "
	@./$(PROG) read com.catbsd.test font
	@echo -n "meow: "
	@./$(PROG) read com.catbsd.test meow
	@echo ""
	@echo "=== Test 3: List domains ==="
	@./$(PROG) domains
	@echo ""
	@echo "=== Test 4: Delete preference ==="
	@./$(PROG) delete com.catbsd.test font
	@echo ""
	@echo "=== Test 5: Verify deletion ==="
	@./$(PROG) read com.catbsd.test theme || true
	@echo ""
	@echo "✓ Tests passed"

.PHONY: all clean test
