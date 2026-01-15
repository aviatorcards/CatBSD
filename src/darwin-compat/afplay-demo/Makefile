# Makefile for afplay-demo

CC=		cc
CFLAGS=		-Wall -Wextra

PROG=		afplay-demo
SRCS=		afplay-demo.c
OBJS=		$(SRCS:.c=.o)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PROG)

test: $(PROG)
	@echo "Testing afplay-demo..."
	@echo ""
	@echo "Note: This requires an audio file to test"
	@echo "If you have an audio file, run:"
	@echo "  ./$(PROG) your-audio-file.mp3"
	@echo "  ./$(PROG) -v 0.5 your-audio-file.mp3"
	@echo ""
	@echo "✓ Build successful"

.PHONY: all clean test
