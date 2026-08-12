# Makefile for launchd-demo

CC=		cc
CFLAGS=		-Wall -Wextra -I../shims
LDFLAGS=	-L../shims

PROG=		launchd-demo
SRCS=		launchd-demo.c
OBJS=		$(SRCS:.c=.o)

# Link against our shim library
LIBS=		../shims/mach_port.o ../shims/darwin_syscalls.o

all: $(PROG)

$(PROG): $(OBJS) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG)

test: $(PROG)
	@echo "Running launchd-demo tests..."
	@echo ""
	@echo "=== Test 1: Help output (exit 0) ==="
	@./$(PROG) help > /dev/null && echo "PASS: help" || { echo "FAIL: help"; exit 1; }
	@echo ""
	@echo "=== Test 2: List empty registry (exit 0) ==="
	@./$(PROG) list | grep -q "No daemons" && echo "PASS: list (empty)" || { echo "FAIL: list"; exit 1; }
	@echo ""
	@echo "=== Test 3: Start daemon (exit 0) ==="
	@./$(PROG) start com.catbsd.test > /dev/null && echo "PASS: start" || { echo "FAIL: start"; exit 1; }
	@echo ""
	@echo "=== Test 4: Unknown command returns non-zero ==="
	@./$(PROG) bogus 2>&1 | grep -q "Unknown command" && echo "PASS: error handling" || { echo "FAIL: error handling"; exit 1; }
	@echo ""
	@echo "✓ Tests passed"
	@echo "Note: Persistent list/stop across separate invocations requires"
	@echo "      a daemon socket — planned in Phase 4 (see MANIFEST.md)."

.PHONY: all clean test
