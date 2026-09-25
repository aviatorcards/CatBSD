/*
 * test_blocks.c — Smoke test for CatBSD libBlocksRuntime
 *
 * Exercises the parts of the runtime that matter for our use:
 *   - stack blocks (the common case, no heap allocation)
 *   - Block_copy / Block_release (heap promotion and refcount)
 *   - variable capture (by value and by reference via __block)
 *   - nested blocks
 *   - blocks as function arguments
 *
 * Compile: cc -fblocks -o test_blocks test_blocks.c -L.. -lBlocksRuntime
 */

#include <Block.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OK "  \xE2\x9C\x93 "

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

typedef int (^int_block_t)(int);
typedef void (^void_block_t)(void);

static int call_with_42(int_block_t b) { return b(42); }

/* ------------------------------------------------------------------ */
/* Tests                                                                */
/* ------------------------------------------------------------------ */

static void test_stack_block(void) {
    printf("Testing stack blocks...\n");

    int x = 10;
    int result = ^(int n) { return x + n; }(5);
    assert(result == 15);
    printf(OK "stack block captured x=%d, returned %d\n", x, result);
}

static void test_block_copy_release(void) {
    printf("Testing Block_copy / Block_release...\n");

    int base = 41;
    int_block_t b = Block_copy(^(int n) { return base + n; });
    assert(b != NULL);

    int r = b(1);
    assert(r == 42);
    printf(OK "heap block returned %d (expected 42)\n", r);

    Block_release(b);
    printf(OK "Block_release did not crash\n");
}

static void test_block_as_argument(void) {
    printf("Testing block as function argument...\n");

    int offset = 100;
    int r = call_with_42(^(int n) { return n + offset; });
    assert(r == 142);
    printf(OK "block argument: 42 + 100 = %d\n", r);
}

static void test_block_variable_capture(void) {
    printf("Testing __block variable capture...\n");

    __block int counter = 0;
    void_block_t inc = Block_copy(^{ counter++; });

    inc(); inc(); inc();
    assert(counter == 3);
    printf(OK "__block counter incremented to %d\n", counter);

    Block_release(inc);
}

static void test_nested_blocks(void) {
    printf("Testing nested blocks...\n");

    int a = 2;
    int_block_t outer = Block_copy(^(int x) {
        int_block_t inner = ^(int y) { return a * x + y; };
        return inner(1);   /* a*x + 1 */
    });

    /* outer(3) -> inner(1) -> 2*3+1 = 7 */
    int r = outer(3);
    assert(r == 7);
    printf(OK "nested blocks: 2*3+1 = %d\n", r);

    Block_release(outer);
}

static void test_block_returning_block(void) {
    printf("Testing block that returns a block...\n");

    /* adder(n) returns a block that adds n to its argument */
    int_block_t (^adder)(int) = ^(int n) {
        return Block_copy(^(int x) { return x + n; });
    };

    int_block_t add5 = adder(5);
    int_block_t add10 = adder(10);

    assert(add5(3) == 8);
    assert(add10(3) == 13);
    printf(OK "add5(3)=%d  add10(3)=%d\n", add5(3), add10(3));

    Block_release(add5);
    Block_release(add10);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("=== CatBSD Blocks Runtime Tests ===\n\n");

    test_stack_block();
    test_block_copy_release();
    test_block_as_argument();
    test_block_variable_capture();
    test_nested_blocks();
    test_block_returning_block();

    printf("\n=== All tests passed! ===\n\n");
    printf("LLVM Blocks Runtime -- stack blocks, heap copy/release,\n"
           "variable capture (by value and __block), nested blocks.\n");
    return 0;
}
