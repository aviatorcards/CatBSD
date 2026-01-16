# Makefile for caffeinate-demo

CC=		cc
CFLAGS=		-Wall -Wextra -I../shims
LDFLAGS=	-L../shims

PROG=		caffeinate-demo
SRCS=		caffeinate-demo.c
OBJS=		$(SRCS:.c=.o)
SHIM_LIB=	../shims/libdarwin_compat.a

all: $(PROG)

$(PROG): $(OBJS) $(SHIM_LIB)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(SHIM_LIB) -lpthread

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG)

test: $(PROG)
	@echo "Testing caffeinate-demo..."
	@echo ""
	@echo "=== Test 1: Timed caffeinate ==="
	@./$(PROG) -t 3
	@echo ""
	@echo "=== Test 2: Run command ==="
	@./$(PROG) sleep 2
	@echo ""
	@echo "✓ Tests passed"

.PHONY: all clean test
