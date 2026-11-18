#include "shell.h"

// Runs INSIDE THE CHILD, just before execvp().
// Finds "<" and ">", opens the file, dup2()s it onto stdin/stdout,
// and removes the operator + filename from args.
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
            dup2(fd, target);      // fd 0 (or 1) now points to the file
            close(fd);             // original fd no longer needed
            i++;                   // skip the filename token
        } else {
            args[out++] = args[i]; // normal argument: keep it
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

// Forks one child that runs "args".
//   in_fd / out_fd : if not -1, become the child's stdin / stdout
//   pipefd         : if not NULL, both pipe ends are closed in the child
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
        perror("Command not found");               // only if execvp failed
        exit(1);
    }
    return pid;                                    // ---- PARENT ----
}

// Runs one command, or two commands joined by one pipe.
int execute(char** arglist) {
    int status = 0;

    int pipe_pos = -1;
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            pipe_pos = i;
            break;
        }
    }

    if (pipe_pos == -1) {                          // no pipe: fork-exec-wait
        pid_t pid = launch(arglist, -1, -1, NULL);
        if (pid < 0) return 1;
        waitpid(pid, &status, 0);
        return exit_code(status);
    }

    char* left[MAXARGS + 1];                       // split WITHOUT modifying arglist
    int n = 0;
    for (; n < pipe_pos; n++) left[n] = arglist[n];
    left[n] = NULL;
    char** right = &arglist[pipe_pos + 1];

    if (left[0] == NULL || right[0] == NULL) {
        fprintf(stderr, "myshell: syntax error near '|'\n");
        return 1;
    }

    int fd[2];                                     // fd[0] = read end, fd[1] = write end
    if (pipe(fd) < 0) {
        perror("pipe failed");
        return 1;
    }

    pid_t p1 = launch(left,  -1,    fd[1], fd);    // writer: stdout -> pipe
    pid_t p2 = launch(right, fd[0], -1,    fd);    // reader: stdin  <- pipe

    close(fd[0]);                                  // parent closes BOTH ends,
    close(fd[1]);                                  // else the reader never sees EOF

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
        printf("  jobs         - show background jobs (not implemented yet)\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        show_history();
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    return 0;   // not a built-in
}
