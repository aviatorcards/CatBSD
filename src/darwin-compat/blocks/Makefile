# CatBSD Blocks Runtime
#
# Builds libBlocksRuntime.a from the LLVM compiler-rt source.
# No external dependencies — just a C compiler with -fblocks support.
#
# Targets:
#   all      build libBlocksRuntime.a
#   test     build and run tests/test_blocks
#   clean    remove build artifacts

CC      = cc
AR      = ar
CFLAGS  = -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
          -I. \
          -fblocks
ARFLAGS = rcs

SRCS = runtime.c
OBJS = $(SRCS:.c=.o)
LIB  = libBlocksRuntime.a

TEST_BIN = tests/test_blocks
TEST_SRC = tests/test_blocks.c

.PHONY: all test clean

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^
	@echo "  AR  $@"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	@echo "  CC  $@"

test: $(TEST_BIN)
	@echo "Running Blocks Runtime tests..."
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -L. -lBlocksRuntime
	@echo "  LD  $@"

clean:
	rm -f $(OBJS) $(LIB) $(TEST_BIN)
