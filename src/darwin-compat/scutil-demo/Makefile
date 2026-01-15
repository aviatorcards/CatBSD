# Makefile for scutil-demo

CC=		cc
CFLAGS=		-Wall -Wextra

PROG=		scutil-demo
SRCS=		scutil-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG)

test: $(PROG)
	@echo "Testing scutil-demo..."
	@echo ""
	@echo "=== Test 1: Show all info ==="
	@./$(PROG)
	@echo ""
	@echo "=== Test 2: Get specific values ==="
	@echo -n "ComputerName: "
	@./$(PROG) --get ComputerName
	@echo -n "HostName: "
	@./$(PROG) --get HostName
	@echo ""
	@echo "✓ Tests passed"

.PHONY: all clean test
