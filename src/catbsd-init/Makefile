# Makefile for catbsd-init
# Builds the PID 1 / service supervisor binary.

CC     = cc
CFLAGS = -Wall -Wextra -Wno-unused-parameter \
         -I../darwin-compat \
         -I../darwin-compat/launchd \
         -I../darwin-compat/shims \
         -DFREEBSD_COMPAT

# Source files from the darwin-compat layer that catbsd-init links directly.
# We compile them into the binary rather than link a .a so the build is
# self-contained (no prior 'make' in each subdir required).
LAUNCHD_SRCS = \
    ../darwin-compat/launchd/launch_job.c \
    ../darwin-compat/launchd/plist_lite.c \
    ../darwin-compat/launchd/launchctl_server.c \
    ../darwin-compat/shims/xpc_shim.c

# pthreads: implicit on Darwin, explicit on FreeBSD/Linux
UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
    PTHREAD =
else
    PTHREAD = -lpthread
endif

BIN = catbsd-init

.PHONY: all test clean install

all: $(BIN)

$(BIN): catbsd-init.c $(LAUNCHD_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(PTHREAD)
	@echo "  LD  $@"

# ---- Smoke tests -------------------------------------------------------
# These run catbsd-init in a non-PID-1 context using environment overrides
# to point at a temp directory.  The daemon itself is killed after a short
# delay with SIGTERM to exercise the clean shutdown path.

TEST_DIR = /tmp/catbsd-init-test-$$$$

test: $(BIN)
	@echo "=== catbsd-init smoke tests ==="
	@echo ""
	@$(MAKE) --no-print-directory _test_empty_dir
	@$(MAKE) --no-print-directory _test_single_user
	@$(MAKE) --no-print-directory _test_daemon_load
	@echo ""
	@echo "=== All catbsd-init smoke tests passed! ==="

# Test 1: empty daemon directory — init should start and accept SIGTERM
_test_empty_dir: $(BIN)
	@echo "Test 1: empty daemon directory + clean SIGTERM shutdown"
	@sh -c ' \
	  T=/tmp/catbsd-init-empty-$$; mkdir -p $$T; \
	  CATBSD_INIT_DAEMON_DIR=$$T CATBSD_INIT_LOG_LEVEL=0 ./$(BIN) & PID=$$!; \
	  sleep 0.3; kill -TERM $$PID; wait $$PID; RC=$$?; \
	  rm -rf $$T; \
	  [ $$RC -eq 0 ] && echo "  ✓ clean SIGTERM exit (status 0)" \
	                 || (echo "  ✗ bad exit status $$RC"; exit 1) \
	'

# Test 2: single-user mode — runs a shell that immediately exits
_test_single_user: $(BIN)
	@echo "Test 2: single-user mode (shell exits 0)"
	@CATBSD_INIT_SINGLE_USER=1 \
	 CATBSD_INIT_SINGLE_USER_SHELL="/bin/sh" \
	 CATBSD_INIT_LOG_LEVEL=0 \
	 ./$(BIN) -s < /dev/null; RC=$$?; \
	 [ $$RC -eq 0 ] && echo "  ✓ single-user shell exited 0" \
	               || (echo "  ✗ bad exit status $$RC"; exit 1)

# Test 3: load a plist dir with one RunAtLoad job, wait, SIGTERM
_test_daemon_load: $(BIN)
	@echo "Test 3: load one RunAtLoad daemon then SIGTERM"
	@T=/tmp/catbsd-init-daemons-$$$$; mkdir -p $$T; \
	 printf '%s\n' \
	   '<?xml version="1.0" encoding="UTF-8"?>' \
	   '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "">' \
	   '<plist version="1.0"><dict>' \
	   '  <key>Label</key><string>com.catbsd.test.sleeper</string>' \
	   '  <key>ProgramArguments</key><array>' \
	   '    <string>/bin/sh</string><string>-c</string>' \
	   '    <string>sleep 30</string>' \
	   '  </array>' \
	   '  <key>RunAtLoad</key><true/>' \
	   '</dict></plist>' > $$T/sleeper.plist; \
	 CATBSD_INIT_DAEMON_DIR=$$T \
	 CATBSD_INIT_LOG_LEVEL=1 \
	 ./$(BIN) 2>&1 & PID=$$!; \
	 sleep 0.5; kill -TERM $$PID; wait $$PID; RC=$$?; \
	 rm -rf $$T; \
	 [ $$RC -eq 0 ] && echo "  ✓ daemon loaded and init exited cleanly" \
	               || (echo "  ✗ bad exit status $$RC"; exit 1)

clean:
	rm -f $(BIN)

install: $(BIN)
	@echo "Install target: copy $(BIN) to /sbin/init (requires root)"
	@echo "  sudo cp $(BIN) /sbin/catbsd-init"
