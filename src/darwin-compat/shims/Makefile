# Makefile for CatBSD Darwin Compatibility Shim Library
# Portable version for macOS and FreeBSD

CC?=	cc
AR?=	ar
RANLIB?=	ranlib

LIB=	libdarwin_compat.a

# Source files
SRCS=	mach_port.c darwin_syscalls.c
OBJS=	$(SRCS:.c=.o)

# Header files
INCS=	mach_port.h darwin_syscalls.h

# Compiler flags
CFLAGS+= -Wall -Wextra
CFLAGS+= -I.
CFLAGS+= -DFREEBSD_COMPAT

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $@ $(OBJS)
	$(RANLIB) $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(LIB)

install: $(LIB)
	@echo "Install not implemented yet"

.PHONY: all clean install
