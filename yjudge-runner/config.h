#define SUID_GROUP "yj-runner"
#define N_SYSCALLS 512
#define INJECTOR_PATH "/usr/local/lib/yjudge-inject.so"
#define FORCE_USER "nobody"
#define FORCE_GROUP "nobody"

#define SYSCALL_ALLOW 1
#define SYSCALL_DENY 2
#define SYSCALL_SV 3

#define CPU_TIME_LIMIT_EXCEEDED 0x01
#define MEMORY_LIMIT_EXCEEDED 0x02
#define REAL_TIME_LIMIT_EXCEEDED 0x04
#define SECURITY_VIOLATION 0x08
