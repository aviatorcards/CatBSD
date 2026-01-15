# Makefile for screencapture-demo

CC=		cc
CFLAGS=		-Wall -Wextra

PROG=		screencapture-demo
SRCS=		screencapture-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG) *.png

test: $(PROG)
	@echo "Testing screencapture-demo..."
	@echo ""
	@echo "Note: Screenshot tests require user interaction"
	@echo "Try these commands:"
	@echo "  ./$(PROG) test.png              # Capture full screen"
	@echo "  ./$(PROG) -i selection.png      # Interactive selection"
	@echo "  ./$(PROG) -c                    # Copy to clipboard"
	@echo ""
	@echo "✓ Build successful"

.PHONY: all clean test
