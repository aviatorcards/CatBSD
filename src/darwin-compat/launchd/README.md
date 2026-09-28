# CatBSD liblaunch

Job definition and process supervision, factored out of `launchd-demo` and
made real: job plists actually parsed, children actually supervised,
`KeepAlive` and `ThrottleInterval` actually honoured, sockets actually
pre-created and handed to the child.

No external dependencies — not even libxml2.

## What this is (and is not)

This is the **supervision core** a launchd is built out of, not a launchd.
There is no PID 1 behaviour, no session or domain model, no XPC bootstrap
namespace and no `launchctl` wire protocol. Those belong on top of this, and
the pieces they need now exist beside it: IPC in `../shims/xpc_shim.h`,
timers in `../libdispatch/dispatch_shim.h`.

`launchd-demo` remains the CLI-shaped demonstration; this is the library it
should eventually sit on.

## Two files

**`plist_lite.{h,c}`** — a self-contained XML plist reader covering
`<dict>`, `<array>`, `<key>`, `<string>`, `<integer>`, `<real>`, `<true/>`,
`<false/>` and base64 `<data>`, with XML entity decoding (including numeric
references, emitted as UTF-8). Not supported: `<date>`, binary plists, and
writing.

Why not libxml2, which `plutil-demo` already links? Because this library is
meant to end up in a base system, where a hard dependency on a large
external parser is a real cost, and job plists use a tiny corner of the
format. Parse errors report the line they gave up on — a bad job plist
should say *where* it is bad.

**`launch_job.{h,c}`** — the job model, the registry, and supervision.

## Supported plist keys

| Key | Effect |
| --- | ------ |
| `Label` | Required. Unique within a registry. |
| `ProgramArguments` | argv. |
| `Program` | Executable, when it differs from `argv[0]`. |
| `RunAtLoad` | Started by `launch_registry_start_all()`. |
| `KeepAlive` | `true`, or `{SuccessfulExit: bool}`. |
| `ThrottleInterval` | Seconds between restarts. |
| `WorkingDirectory` | `chdir` before exec. |
| `EnvironmentVariables` | Merged into the child's environment. |
| `StandardInPath` / `StandardOutPath` / `StandardErrorPath` | fd redirection. |
| `Sockets` | `{Name: {SockPathName: path}}` — Unix domain only. |

Unrecognised keys are ignored rather than rejected, so a plist copied off a
Mac still loads. A `Sockets` entry with no `SockPathName` *is* rejected,
because silently ignoring it would leave a socket the job expects unbound.

## Supervision is a tick, not a signal handler

```c
launch_registry_t *reg = launch_registry_create();
launch_registry_load_dir(reg, "/etc/catbsd/daemons", &failed, err, sizeof err);
launch_registry_start_all(reg);

for (;;) {
    launch_registry_tick(reg);   /* reap, apply KeepAlive, restart */
    usleep(100000);
}
```

The caller decides when to tick. A library that installed its own `SIGCHLD`
handler would fight whatever the embedding program already does with
signals, and the tick model drops straight into an existing event loop or a
`catbsd_dispatch_source_create_timer()`.

`tick()` reaps with `waitpid(job->pid, ...)` per job, never `waitpid(-1)`:
this library may be embedded in a program with children of its own, and
reaping those would break it in a way that is very hard to debug.

## Restart policy

| KeepAlive | Restarts when |
| --------- | ------------- |
| absent / `false` | never |
| `true` | always |
| `{SuccessfulExit: false}` | the job exited non-zero or was signalled |
| `{SuccessfulExit: true}` | the job exited zero |

A job stopped with `launch_job_stop()` moves to `STOPPED` and is **not**
restarted, whatever its `KeepAlive` says — an operator's stop outranks the
plist.

`launch_job_stop()` sends `SIGTERM`, waits up to the caller's timeout, then
escalates to `SIGKILL`. A job that ignores `SIGTERM` must not be able to
keep the supervisor waiting forever.

`ThrottleInterval` is what stops a job that exits immediately from
respawning as fast as `fork()` allows. The plist expresses it in whole
seconds; `launch_registry_set_default_throttle()` overrides the default in
milliseconds, which is what makes restart behaviour testable in under a
second instead of Darwin's 10.

## Socket activation

Sockets named in the job's `Sockets` dict are bound and listening **before**
the child is forked, so a client can connect the instant the job is loaded
without racing its startup. The descriptor number reaches the child through
the environment:

```
LAUNCH_ACTIVATE_SOCKET_FD_<NAME>=<fd>
```

and the child recovers it with `launch_activate_socket("Name")`, which is
the shape Darwin's function of the same name has. `FD_CLOEXEC` is cleared on
those descriptors just before `exec`.

## A note on fork safety

The child of a `fork()` in a process that may have other threads can only
safely call async-signal-safe functions — and `setenv()` is not one, because
it allocates. So the full environment block is assembled **in the parent**,
before the fork, and the child does nothing but `chdir`, `dup2`, `fcntl`,
point `environ` at the prepared block, and `execvp`.

## Building

```sh
make          # builds liblaunch.a
make test     # builds and runs tests/test_launch
make clean
```

## Tests

`tests/test_launch.c` spawns real `/bin/sh` children, because the behaviour
worth proving only exists once there is a process to supervise:

- an instantly-exiting `KeepAlive` job is asserted to restart a bounded
  number of times in half a second — proving the throttle, not just the
  restart
- `{SuccessfulExit: false}` is asserted to leave a clean-exiting job down
  while restarting a failing one, in the same registry, in the same run
- a job that traps `SIGTERM` is asserted to die by `SIGKILL` within the
  grace period
- unloading a running job is asserted to have actually killed its pid, not
  orphaned it
- a client is asserted to connect to an activation socket while the job is
  still starting, and the child's inherited fd number is asserted to match
  the parent's
- a malformed plist is asserted to report a line number

Verified clean under `-fsanitize=address,undefined`.

## Status

| Property | State |
| -------- | ----- |
| Builds | macOS, FreeBSD, Linux |
| Tested | Linux/aarch64, gcc 11. **Not yet run on macOS or FreeBSD.** |
| Dependencies | none beyond libc |

Everything here is POSIX (`fork`, `execvp`, `waitpid`, `kill`, AF_UNIX
sockets, `dirent`), so the FreeBSD and macOS builds should be uneventful —
but they have not been run yet.
