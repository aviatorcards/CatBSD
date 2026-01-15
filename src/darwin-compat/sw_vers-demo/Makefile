# Makefile for sw_vers-demo

CC=		cc
CFLAGS=		-Wall -Wextra

PROG=		sw_vers-demo
SRCS=		sw_vers-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG)

test: $(PROG)
	@echo "Testing sw_vers-demo..."
	@echo ""
	@echo "=== Full output ==="
	@./$(PROG)
	@echo ""
	@echo "=== Individual fields ==="
	@echo -n "Product: "
	@./$(PROG) -productName
	@echo -n "Version: "
	@./$(PROG) -productVersion
	@echo -n "Build: "
	@./$(PROG) -buildVersion
	@echo ""
	@echo "=== CatBSD Banner ==="
	@./$(PROG) -banner
	@echo "✓ Tests passed"

.PHONY: all clean test
