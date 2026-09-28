# Makefile for CatBSD liblaunch
# Portable version for macOS, FreeBSD and Linux

CC?=	cc
AR?=	ar
RANLIB?=	ranlib

LIB=	liblaunch.a

SRCS=	launch_job.c plist_lite.c launchctl_server.c
OBJS=	$(SRCS:.c=.o)

INCS=	launch_job.h plist_lite.h launchctl_server.h launchctl_proto.h

CFLAGS+= -Wall -Wextra
CFLAGS+= -I.
CFLAGS+= -I../shims
CFLAGS+= -DFREEBSD_COMPAT

# No external dependencies: the plist reader is deliberately self-contained
# so a base-system supervisor doesn't have to pull in libxml2.
# launchctl_server.c depends on xpc_shim.c (transport layer).
XPC_SRCS = ../shims/xpc_shim.c

# pthreads: implicit on Darwin, explicit on FreeBSD/Linux
UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
    PTHREAD =
else
    PTHREAD = -lpthread
endif

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $@ $(OBJS)
	$(RANLIB) $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

test: $(LIB)
	@echo "Building launch test suite..."
	$(CC) $(CFLAGS) -o tests/test_launch tests/test_launch.c \
		launch_job.c plist_lite.c $(XPC_SRCS) $(PTHREAD)
	@echo ""
	@./tests/test_launch

test-launchctl: $(LIB)
	@echo "Building launchctl integration test..."
	$(CC) $(CFLAGS) -o tests/test_launchctl tests/test_launchctl.c \
		$(SRCS) $(XPC_SRCS) $(PTHREAD)
	@echo ""
	@./tests/test_launchctl

clean:
	rm -f $(OBJS) $(LIB) tests/test_launch tests/test_launchctl
	rm -rf /tmp/catbsd-launch-test-* /tmp/catbsd-launchctl-test-*

install: $(LIB)
	@echo "Install not implemented yet"

.PHONY: all clean install test test-launchctl
