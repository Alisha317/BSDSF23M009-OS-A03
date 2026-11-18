#include "shell.h"

// Runs INSIDE THE CHILD, just before execvp().
static void apply_redirections(char** args) {
    int out = 0;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "myshell: syntax error near '%s'\n", args[i]);
                exit(1);
            }
            int fd, target;
            if (args[i][0] == '<') {
                fd = open(args[i + 1], O_RDONLY);
                target = STDIN_FILENO;
            } else {
                fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                target = STDOUT_FILENO;
            }
            if (fd < 0) {
                perror(args[i + 1]);
                exit(1);
            }
            dup2(fd, target);
            close(fd);
            i++;
        } else {
            args[out++] = args[i];
        }
    }
    args[out] = NULL;
}

// Converts a wait() status into an exit code
static int exit_code(int status) {
    if (WIFEXITED(status))   return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

// ---------------- Background job table ----------------
#define MAX_JOBS 32
#define JOB_CMD_LEN 256
typedef struct {
    pid_t pid;
    char  cmd[JOB_CMD_LEN];
} Job;

static Job jobs[MAX_JOBS];
static int njobs = 0;

static void add_job(pid_t pid, char** args) {
    if (njobs >= MAX_JOBS) {
        fprintf(stderr, "myshell: too many background jobs\n");
        return;
    }
    jobs[njobs].pid = pid;
    jobs[njobs].cmd[0] = '\0';
    for (int i = 0; args[i] != NULL; i++) {
        strncat(jobs[njobs].cmd, args[i], JOB_CMD_LEN - strlen(jobs[njobs].cmd) - 2);
        strncat(jobs[njobs].cmd, " ", JOB_CMD_LEN - strlen(jobs[njobs].cmd) - 1);
    }
    njobs++;
    printf("[%d] %d\n", njobs, (int)pid);
}

// Called before every prompt: reap ALL finished children without blocking
void reap_background(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < njobs; i++) {
            if (jobs[i].pid == pid) {
                printf("[done] %d  %s\n", (int)pid, jobs[i].cmd);
                for (int j = i + 1; j < njobs; j++)
                    jobs[j - 1] = jobs[j];
                njobs--;
                break;
            }
        }
    }
}

static void print_jobs(void) {
    if (njobs == 0) {
        printf("No background jobs.\n");
        return;
    }
    for (int i = 0; i < njobs; i++)
        printf("[%d] %d  Running  %s\n", i + 1, (int)jobs[i].pid, jobs[i].cmd);
}

// Forks one child that runs "args".
static pid_t launch(char** args, int in_fd, int out_fd, int* pipefd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return -1;
    }
    if (pid == 0) {                                // ---- CHILD ----
        if (in_fd  != -1) dup2(in_fd,  STDIN_FILENO);
        if (out_fd != -1) dup2(out_fd, STDOUT_FILENO);
        if (pipefd != NULL) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        apply_redirections(args);
        execvp(args[0], args);
        perror("Command not found");
        exit(1);
    }
    return pid;                                    // ---- PARENT ----
}

// Runs one command, or two commands joined by one pipe.
int execute(char** arglist, int background) {
    int status = 0;

    int pipe_pos = -1;
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            pipe_pos = i;
            break;
        }
    }

    if (pipe_pos == -1) {
        pid_t pid = launch(arglist, -1, -1, NULL);
        if (pid < 0) return 1;
        if (background) {                          // do NOT wait
            add_job(pid, arglist);
            return 0;
        }
        waitpid(pid, &status, 0);
        return exit_code(status);
    }

    char* left[MAXARGS + 1];
    int n = 0;
    for (; n < pipe_pos; n++) left[n] = arglist[n];
    left[n] = NULL;
    char** right = &arglist[pipe_pos + 1];

    if (left[0] == NULL || right[0] == NULL) {
        fprintf(stderr, "myshell: syntax error near '|'\n");
        return 1;
    }

    int fd[2];
    if (pipe(fd) < 0) {
        perror("pipe failed");
        return 1;
    }

    pid_t p1 = launch(left,  -1,    fd[1], fd);
    pid_t p2 = launch(right, fd[0], -1,    fd);

    close(fd[0]);
    close(fd[1]);

    if (background) {
        add_job(p2, right);
        return 0;
    }
    waitpid(p1, NULL, 0);
    waitpid(p2, &status, 0);
    return exit_code(status);
}

// ---------------- Built-in commands ----------------
int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL)
        return 0;

    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) {
            char* home = getenv("HOME");
            if (home != NULL && chdir(home) != 0)
                perror("cd");
        } else if (chdir(arglist[1]) != 0) {
            perror("cd");
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit         - terminate the shell\n");
        printf("  cd <dir>     - change the current directory\n");
        printf("  help         - show this help message\n");
        printf("  jobs         - show background jobs\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        show_history();
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }

    return 0;   // not a built-in
}
