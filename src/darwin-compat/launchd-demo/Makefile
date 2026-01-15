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
	@./$(PROG) help
	@echo ""
	@echo "Starting test daemon..."
	@./$(PROG) start com.catbsd.test &
	@sleep 1
	@./$(PROG) list
	@./$(PROG) stop com.catbsd.test
	@echo "✓ Tests passed"

.PHONY: all clean test
