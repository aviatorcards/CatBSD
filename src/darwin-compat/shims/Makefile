# Makefile for CatBSD Darwin Compatibility Shim Library
# Portable version for macOS and FreeBSD

CC?=	cc
AR?=	ar
RANLIB?=	ranlib

LIB=	libdarwin_compat.a

# Source files
SRCS=	mach_port.c darwin_syscalls.c xpc_shim.c
OBJS=	$(SRCS:.c=.o)

# Header files
INCS=	mach_port.h darwin_syscalls.h xpc_shim.h xpc_compat.h

# Compiler flags
CFLAGS+= -Wall -Wextra
CFLAGS+= -I.
CFLAGS+= -DFREEBSD_COMPAT

# The XPC shim spawns a reader thread per resumed connection, so anything
# linking this library needs pthreads.
LDLIBS+= -lpthread

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $@ $(OBJS)
	$(RANLIB) $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

test: $(LIB)
	@cd tests && ./run_tests.sh

clean:
	rm -f $(OBJS) $(LIB) tests/test_shims tests/test_xpc

install: $(LIB)
	@echo "Install not implemented yet"

.PHONY: all clean install test
