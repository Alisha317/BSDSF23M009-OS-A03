#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {

        // Empty line: nothing to run and nothing to store
        if (cmdline[0] == '\0') {
            free(cmdline);
            continue;
        }

        // "!n" must be handled BEFORE tokenizing and BEFORE adding to history
        if (cmdline[0] == '!') {
            int n = atoi(cmdline + 1);              // number after '!'
            char* old = get_history(n);
            if (old == NULL) {
                fprintf(stderr, "myshell: !%s: event not found\n", cmdline + 1);
                free(cmdline);
                continue;
            }
            free(cmdline);
            cmdline = strdup(old);                  // copy it, then treat as typed input
            printf("%s\n", cmdline);                // echo the command being re-run
        }

        add_to_history(cmdline);

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (handle_builtin(arglist) == 0)
                execute(arglist);

            for (int i = 0; arglist[i] != NULL; i++)
                free(arglist[i]);
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
