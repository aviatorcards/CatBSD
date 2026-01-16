# Makefile for plutil-demo

CC=		cc
CFLAGS=		-Wall -Wextra

# Platform-specific libxml2 paths
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -I/opt/homebrew/opt/libxml2/include/libxml2
    LDFLAGS = -L/opt/homebrew/opt/libxml2/lib
else ifeq ($(UNAME_S),FreeBSD)
    CFLAGS += -I/usr/local/include/libxml2
    LDFLAGS = -L/usr/local/lib
endif
LIBS=		-lxml2

PROG=		plutil-demo
SRCS=		plutil-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG)

test: $(PROG)
	@echo "Creating test plist..."
	@echo '<?xml version="1.0" encoding="UTF-8"?>' > test.plist
	@echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' >> test.plist
	@echo '<plist version="1.0">' >> test.plist
	@echo '<dict>' >> test.plist
	@echo '  <key>Label</key>' >> test.plist
	@echo '  <string>com.catbsd.test</string>' >> test.plist
	@echo '  <key>ProgramArguments</key>' >> test.plist
	@echo '  <array>' >> test.plist
	@echo '    <string>/usr/bin/test</string>' >> test.plist
	@echo '  </array>' >> test.plist
	@echo '  <key>RunAtLoad</key>' >> test.plist
	@echo '  <true/>' >> test.plist
	@echo '</dict>' >> test.plist
	@echo '</plist>' >> test.plist
	@echo ""
	@echo "Testing plutil-demo..."
	@./$(PROG) -lint test.plist
	@echo ""
	@./$(PROG) -p test.plist
	@echo "✓ Tests passed"

.PHONY: all clean test
