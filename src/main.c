#include "shell.h"

int main() {
    char* cmdline;

    while (1) {
        reap_background();

        cmdline = read_cmd(PROMPT, stdin);
        if (cmdline == NULL)
            break;

        if (cmdline[0] == '\0') {
            free(cmdline);
            continue;
        }

        if (cmdline[0] == '!') {
            int n = atoi(cmdline + 1);
            char* old = get_history(n);
            if (old == NULL) {
                fprintf(stderr, "myshell: !%s: event not found\n", cmdline + 1);
                free(cmdline);
                continue;
            }
            free(cmdline);
            cmdline = strdup(old);
            printf("%s\n", cmdline);
        }

        add_to_history(cmdline);

        // "if" starts a multi-line block
        if (strncmp(cmdline, "if", 2) == 0 && (cmdline[2] == ' ' || cmdline[2] == '\0')) {
            handle_if(cmdline);
            free(cmdline);
            continue;
        }

        run_line(cmdline);
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}