# Makefile for catbsd-launchctl demo binary

CC     = cc
CFLAGS = -Wall -Wextra -Wno-unused-parameter \
         -I.. \
         -DFREEBSD_COMPAT

# XPC shim (transport)
XPC_DIR  = ../shims
XPC_SRCS = $(XPC_DIR)/xpc_shim.c

# On macOS the system provides pthreads in libc; on FreeBSD/Linux link
# explicitly.
UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
    LDFLAGS =
else
    LDFLAGS = -lpthread
endif

BIN = catbsd-launchctl

.PHONY: all clean

all: $(BIN)

$(BIN): launchctl.c $(XPC_SRCS)
	$(CC) $(CFLAGS) -o $@ launchctl.c $(XPC_SRCS) $(LDFLAGS)
	@echo "  LD  $@"

clean:
	rm -f $(BIN)
