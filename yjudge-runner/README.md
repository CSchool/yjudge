# yjudge-runner

This is a utility for internal use.
It starts a new process as user in a ptrace jail
with limits, measures time and memory usage and
reports it back.

## Usage

    $ yjudge-runner <params> -- <command> [args...]

Each parameter looks like `KEY=VALUE`.

## Parameters

 - `cpu_limit`: cpu time limit, in milliseconds, 0 = unlimited
 - `real_limit`: real time limit, in milliseconds, 0 = unlimited
 - `mem_limit`: memory limit, in KiB, 0 = unlimited
 - `user`: user login to run as (requires root)
 - `group`: group name to run as (requires root)
 - `log_path`: path to write log to
 - `syscalls_default`: `allow`, `deny`, `sv`
 - `syscalls_allow`: comma-separated list of syscall numbers to allow
 - `syscalls_deny`: syscalls to return EPERM
 - `syscalls_sv`: syscalls to report Security Violation

## Usage recommendations

 - Make yjudge-runner binary a suid root, only users in
   `yj-runner` group will be able to use it.
 - Restrict user and group to `nobody` in `config.h`
 - Use `yj-runner` in a gvisor container.
 - Do not use it to run statically linked executables,
   as `LD_PRELOAD` is used to forbid syscalls.
 - Use `log_path`, so that the executing program can't interfere with `stderr`.

