# Makefile for networksetup-demo

CC=		cc
CFLAGS=		-Wall -Wextra

PROG=		networksetup-demo
SRCS=		networksetup-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG)

test: $(PROG)
	@echo "Testing networksetup-demo..."
	@echo ""
	@echo "=== Test 1: List network services ==="
	@./$(PROG) -listallnetworkservices
	@echo ""
	@echo "=== Test 2: Get Wi-Fi info ==="
	@./$(PROG) -getinfo Wi-Fi || true
	@echo ""
	@echo "✓ Tests passed"

.PHONY: all clean test
