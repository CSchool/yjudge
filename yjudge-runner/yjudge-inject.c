#include "config.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <linux/bpf.h>
#include <linux/seccomp.h>
#include <linux/filter.h>

int syscall_actions[N_SYSCALLS];
#define FILTER_SIZE (2 * N_SYSCALLS + 2)

struct sock_filter filter[FILTER_SIZE];

void yjudge_inject() {
    char *env = getenv("YJUDGE_INJECT_SYSCALLS");

    if (!env || (strlen(env) < N_SYSCALLS)) {
        fprintf(stderr, "YJUDGE_INJECT_SYSCALLS is invalid\n");
        exit(EXIT_FAILURE);
    }

    unsetenv("LD_PRELOAD");
    unsetenv("YJUDGE_INJECT_SYSCALLS");

    for (int i = 0; i < N_SYSCALLS; ++i) {
        syscall_actions[i] = env[i] - 'A' + 1;
    }

    /* Set up the filter */
    int i = 0;

    /* Load syscall number into accumulator */
    filter[i++] = (struct sock_filter) BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof (struct seccomp_data, nr)));

    for (int sys = 0; sys < N_SYSCALLS; ++sys) {
        filter[i++] = (struct sock_filter) BPF_JUMP(BPF_JMP | BPF_JEQ, sys, 0, 1);
        if (syscall_actions[sys] == SYSCALL_ALLOW) {
            filter[i++] = (struct sock_filter) BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);
        } else if (syscall_actions[sys] == SYSCALL_SV) {
            filter[i++] = (struct sock_filter) BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL);
        } else {
            filter[i++] = (struct sock_filter) BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA));
        }
    }
    filter[i++] = (struct sock_filter) BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA));

    struct sock_fprog prog;
    prog.len = (unsigned short)i;
    prog.filter = (struct sock_filter *)filter;

    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0)) {
        perror("prctl(PR_SET_SECCOMP)");
        exit(EXIT_FAILURE);
    }
}
