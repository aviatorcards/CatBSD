# catbsd-init — CatBSD PID 1 / Service Supervisor

The init process for CatBSD. Loads job plists, supervises daemons,
and answers `catbsd-launchctl` requests over the XPC control socket.

## Quick Start (non-PID-1, for development)

```bash
# Build
cd src/catbsd-init && make

# Run with the bundled example daemons (log goes to stderr)
CATBSD_INIT_DAEMON_DIR=../../etc/catbsd/daemons ./catbsd-init

# In another terminal
./catbsd-launchctl list          # see running jobs
./catbsd-launchctl stop com.catbsd.syslog
./catbsd-launchctl start com.catbsd.syslog

# Graceful shutdown
kill -TERM <pid>
```

## Boot Sequence

```
1. Read catbsd-init.conf (or env overrides)
2. Set up signal handlers (SIGTERM/SIGINT → shutdown, SIGHUP → reload)
3. Mount /proc and /dev  ← skipped when not PID 1
4. Load *.plist from DaemonDirectory
5. Start all RunAtLoad jobs
6. Open launchctl control socket (XPC over AF_UNIX)
7. Supervision loop:
     poll for launchctl requests (10 ms timeout)
     tick job supervisor (reap + restart children)
     sleep 10 ms
8. SIGTERM/SIGINT: stop all jobs → exit 0
```

## Configuration

Read from `/etc/catbsd/catbsd-init.conf` (plist format).
Override any key with `CATBSD_INIT_<KEY>=value` in the environment.

| Key | Default | Description |
|-----|---------|-------------|
| `DaemonDirectory` | `/etc/catbsd/daemons` | Scanned for `*.plist` at boot |
| `ControlSocket` | *(XPC shim default)* | AF_UNIX socket for `catbsd-launchctl` |
| `SingleUser` | `false` | Drop to shell instead of loading daemons |
| `SingleUserShell` | `/bin/sh` | Shell used in single-user mode |
| `ShutdownTimeoutMs` | `5000` | Grace period before SIGKILL on shutdown |
| `LogLevel` | `1` | 0=quiet 1=normal 2=verbose 3=debug |

## Single-User Mode

```bash
# Via flag
./catbsd-init -s

# Via environment
CATBSD_INIT_SINGLE_USER=1 ./catbsd-init

# On a real FreeBSD system (kernel command line)
# boot: catbsd-init -s
```

Drops to `SingleUserShell` without loading any daemons. When the shell
exits, `catbsd-init` exits too (exit code mirrors the shell's).

## Signal Handling

| Signal | Effect |
|--------|--------|
| `SIGTERM` | Graceful shutdown: stop all jobs, then exit 0 |
| `SIGINT` | Same as SIGTERM (useful in non-PID-1 testing) |
| `SIGHUP` | Reload DaemonDirectory — loads new plists, leaves existing jobs alone |
| `SIGCHLD` | `SIG_DFL` — kernel queues it; `launch_registry_tick()` handles reaping |
| `SIGPIPE` | Ignored — closed launchctl connections must not kill init |

## FreeBSD Installation

```bash
make
sudo cp catbsd-init /sbin/catbsd-init
# Add to /boot/loader.conf:
#   init_path="/sbin/catbsd-init"
```

> ⚠️ **Alpha software**: use a VM. Replacing init on a live system will
> require a reboot to take effect; a bad init binary requires booting from
> external media to recover.

## Dependencies

Built directly from source — no pre-built `.a` files required:

```
catbsd-init.c
  → launchd/launch_job.{c,h}      job supervision
  → launchd/plist_lite.{c,h}      config + plist reading
  → launchd/launchctl_server.{c,h} control socket
  → launchd/launchctl_proto.h      wire protocol
  → shims/xpc_shim.{c,h}          IPC transport
```

## Status

| Property | State |
|----------|-------|
| Builds | macOS (clang), expected FreeBSD |
| Smoke tests | 3/3 pass on macOS |
| FreeBSD | Build should work; PID 1 behaviour untested |
| GUI | Not planned for this milestone |
