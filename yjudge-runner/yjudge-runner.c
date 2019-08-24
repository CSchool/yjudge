#include "config.h"
#include "runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/reg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/time.h>

#define REG_SYSCALL (sizeof(long) * ORIG_RAX)
#define REG_RESULT (sizeof(long) * RAX)

extern char **environ;

int cpu_limit = 0;
int real_limit = 0;
int mem_limit = 0;

char *as_user = NULL;
char *as_group = NULL;
uid_t as_uid;
gid_t as_gid;
char *log_path = NULL;

int syscall_default = SYSCALL_DENY;
int syscall_actions[N_SYSCALLS];

int targc = 0;
char **targv = NULL;

int pipe_up[2];    /* master reads, slave writes */
int pipe_down[2];  /* slave reads, master writes */

int flags = 0;
int exit_code = 0;

int cpu_time = 0;
int real_time = 0;
int memory_used = 0;

pid_t pid;

static void drop_uid() {
    if (geteuid() != 0) return;
    uid_t uid = getuid();
    uid_t gid = getgid();

    setgroups(0, NULL);
    setgid(gid);
    setuid(uid);
}

static void set_limit(enum __rlimit_resource limit, rlim_t soft, rlim_t hard)
{
    struct rlimit lim = {
            soft,
            hard,
    };
    setrlimit(limit, &lim);
}

static void slave() {
    close(pipe_up[0]);
    close(pipe_down[1]);

    int rd = pipe_down[0];
    int wr = pipe_up[1];

    int sync;
    read(rd, &sync, sizeof sync); /* wait for master */
    close(rd);
    fcntl(wr, F_SETFD, FD_CLOEXEC);

    if (geteuid() == 0) {
        if (as_group) {
            setgroups(0, NULL);
            if (setgid(as_gid) == -1) goto err;
        }

        if (as_user) {
            setgroups(0, NULL);
            if (setuid(as_uid) == -1) goto err;
        }
    }

    if (cpu_limit) {
        int sec = (cpu_limit + 999) / 1000;
        set_limit(RLIMIT_CPU, sec, sec + 1);  /* We want to catch SIGXCPU */
    }

    if (mem_limit) {
        long bytes = (long)mem_limit * 1024;
        set_limit(RLIMIT_AS, bytes, bytes);
    }

    int n_env = 0;
    for (; environ[n_env]; ++n_env);
    n_env += 2; /* LD_PRELOAD and YJUDGE_INJECT_SYSCALLS */

    char **envp = malloc(sizeof(char *) * (n_env + 1));
    if (!envp) goto err;
    int i = 0;
    for (char **e = environ; *e; ++e) {
        if (!strncmp("LD_PRELOAD=", *e, sizeof "LD_PRELOAD")) {
            continue;
        }

        if (!strncmp("YJUDGE_INJECT_SYSCALLS=", *e, sizeof "YJUDGE_INJECT_SYSCALLS")) {
            continue;
        }

        envp[i] = strdup(*e);
        if (!envp[i]) goto err;
        ++i;
    }
    envp[i++] = "LD_PRELOAD=" INJECTOR_PATH;
    envp[i] = malloc((sizeof "YJUDGE_INJECT_SYSCALLS") + N_SYSCALLS + 1);
    if (!envp[i]) goto err;
    strcpy(envp[i], "YJUDGE_INJECT_SYSCALLS=");

    for (int j = 0; j < N_SYSCALLS; ++j) {
        envp[i][j + (sizeof "YJUDGE_INJECT_SYSCALLS")] = (char)('A' + syscall_actions[j] - 1);
    }

    envp[i][sizeof "YJUDGE_INJECT_SYSCALLS" + N_SYSCALLS] = '\0';
    envp[++i] = NULL;

    if (!prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
        execve(targv[0], targv, envp);
    }

err:;
    int err = errno;
    write(wr, &err, sizeof err);
    close(wr);

    exit(EXIT_FAILURE);
}

static void alarm_handler(int sig) {
    (void)sig;
    kill(pid, SIGKILL);
    flags |= REAL_TIME_LIMIT_EXCEEDED;
}

long get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return 1000 * tv.tv_sec + tv.tv_usec / 1000;
}

static void master() {
    umask(0077);
    close(pipe_up[1]);
    close(pipe_down[0]);

    int wr = pipe_down[1];
    int rd = pipe_up[0];

    if (real_limit) {
        if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
            perror("signal(SIGALRM)");
            kill(pid, SIGKILL);
            exit(EXIT_FAILURE);
        }
        struct itimerval it_val;
        it_val.it_interval.tv_sec = 0;
        it_val.it_interval.tv_usec = 0;
        it_val.it_value.tv_sec = real_limit / 1000;
        it_val.it_value.tv_usec = (real_limit % 1000) * 1000;
        if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
            perror("setitimer()");
            kill(pid, SIGKILL);
            exit(EXIT_FAILURE);
        }
    }

    long start = get_time();

    int sync = 0;
    write(wr, &sync, sizeof sync);
    close(wr);

    if (read(rd, &sync, sizeof sync) > 0) {
        fprintf(stderr, "exec failed: %s\n", strerror(sync));
        exit(EXIT_FAILURE);
    }

    close(rd);

    int status;
    struct rusage ru;

    wait4(pid, &status, 0, &ru);

    if (WIFSIGNALED(status)) {
        if (WTERMSIG(status) == SIGXCPU) {
            flags |= CPU_TIME_LIMIT_EXCEEDED;
        }

        if (WTERMSIG(status) == SIGSYS) {
            flags |= SECURITY_VIOLATION;
        }

        exit_code = -WTERMSIG(status);
    } else if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    }

    cpu_time =  ru.ru_utime.tv_sec * 1000 + ru.ru_utime.tv_usec / 1000;
    cpu_time += ru.ru_stime.tv_sec * 1000 + ru.ru_stime.tv_usec / 1000;
    memory_used = ru.ru_maxrss;
    real_time = get_time() - start;

    drop_uid();
    FILE *f = stderr;

    if (log_path) {
        f = fopen(log_path, "w");
        if (!f) {
            perror("fopen()");
            exit(EXIT_FAILURE);
        }
    }

    fprintf(f, "flags: %d\n", flags);
    fprintf(f, "cpu: %d\n", cpu_time);
    fprintf(f, "real: %d\n", real_time);
    fprintf(f, "memory: %d\n", memory_used);
    fprintf(f, "exitcode: %d\n", exit_code);

    if (log_path) {
        fclose(f);
    }
}

static void make_pipes() {
    pipe(pipe_up);
    pipe(pipe_down);
}

static void execute() {
    make_pipes();
    pid = fork();

    if (pid < 0) {
        perror("fork()");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        slave();
    }

    master();
}

static void check_suid() {
    uid_t uid = getuid();
    if (geteuid() == 0 && uid != 0) {
        struct passwd* passwd = getpwuid(uid);

        if (!passwd) {
            perror("getpwuid()");
            exit(EXIT_FAILURE);
        }

        struct group* group = getgrnam(SUID_GROUP);

        if (!group) {
            fprintf(stderr, "Group '%s' does not exist\n", SUID_GROUP);
            exit(EXIT_FAILURE);
        }

        gid_t req_gid = group->gr_gid;

        int ngroups = 0;

        getgrouplist(passwd->pw_name, passwd->pw_gid, NULL, &ngroups);

        if (!ngroups) {
            fprintf(stderr, "Not in group '%s'\n", SUID_GROUP);
            exit(EXIT_FAILURE);
        }

        gid_t *groups = malloc(sizeof(gid_t) * ngroups);

        if (!groups) {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }

        if (getgrouplist(passwd->pw_name, passwd->pw_gid, groups, &ngroups) == -1) {
            fprintf(stderr, "Not in group '%s'\n", SUID_GROUP);
            exit(EXIT_FAILURE);
        }

        int good = 0;

        for (int i = 0; i < ngroups; ++i) {
            if (groups[i] == req_gid) {
                good = 1;
                break;
            }
        }

        if (!good) {
            fprintf(stderr, "Not in group '%s'\n", SUID_GROUP);
            exit(EXIT_FAILURE);
        }

        free(groups);
    }
}

static void parse_cmdline(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        char *key = argv[i];
        char *value = strchr(key, '=');

        if (!value) {
            if (strcmp(key, "--")) {
                fprintf(stderr, "Invalid usage\n");
                exit(EXIT_FAILURE);
            }
            ++i;
            targc = argc - i;
            targv = argv + i;
            break;
        }

        *value = '\0';
        ++value;

        if (!strcmp(key, "cpu_limit")) {
            cpu_limit = atoi(value);
        } else if (!strcmp(key, "real_limit")) {
            real_limit = atoi(value);
        } else if (!strcmp(key, "mem_limit")) {
            mem_limit = atoi(value);
        } else if (!strcmp(key, "user")) {
            as_user = value;
        } else if (!strcmp(key, "group")) {
            as_group = value;
        } else if (!strcmp(key, "log_path")) {
            log_path = value;
        } else if (!strcmp(key, "syscalls_default")) {
            if (!strcmp(value, "allow")) {
                syscall_default = SYSCALL_ALLOW;
            } else if (!strcmp(value, "deny")) {
                syscall_default = SYSCALL_DENY;
            } else if (!strcmp(value, "sv")) {
                syscall_default = SYSCALL_SV;
            }
        } else if (!strcmp(key, "syscalls_allow")) {
            for (char *tok = strtok(value, ","); tok; tok = strtok(NULL, ",")) {
                syscall_actions[atoi(tok)] = SYSCALL_ALLOW;
            }
        } else if (!strcmp(key, "syscalls_deny")) {
            for (char *tok = strtok(value, ","); tok; tok = strtok(NULL, ",")) {
                syscall_actions[atoi(tok)] = SYSCALL_DENY;
            }
        } else if (!strcmp(key, "syscalls_sv")) {
            for (char *tok = strtok(value, ","); tok; tok = strtok(NULL, ",")) {
                syscall_actions[atoi(tok)] = SYSCALL_SV;
            }
        }
    }

    for (int i = 0; i < N_SYSCALLS; ++i) {
        if (!syscall_actions[i]) {
            syscall_actions[i] = syscall_default;
        }
    }

    if (!targv) {
        fprintf(stderr, "Invalid usage\n");
        exit(EXIT_FAILURE);
    }

#ifdef FORCE_USER
    as_user = FORCE_USER;
#endif

#ifdef FORCE_GROUP
    as_group = FORCE_GROUP;
#endif

    if (as_user) {
        struct passwd *passwd = getpwnam(as_user);
        if (!passwd) {
            fprintf(stderr, "Invalid user: %s\n", as_user);
            exit(EXIT_FAILURE);
        }
        as_uid = passwd->pw_uid;
    }

    if (as_group) {
        struct group *group = getgrnam(as_group);
        if (!group) {
            fprintf(stderr, "Invalid group: %s\n", as_group);
            exit(EXIT_FAILURE);
        }
        as_gid = group->gr_gid;
    }
}

int main(int argc, char **argv) {
    check_suid();
    parse_cmdline(argc, argv);
    execute();
    return EXIT_SUCCESS;
}
