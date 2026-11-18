#include "shell.h"

int main() {
    char* cmdline;

    while (1) {
        reap_background();                          // collect zombies BEFORE the prompt

        cmdline = read_cmd(PROMPT, stdin);
        if (cmdline == NULL)                        // Ctrl+D
            break;

        if (cmdline[0] == '\0') {
            free(cmdline);
            continue;
        }

        if (cmdline[0] == '!') {                    // history re-execution
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
        run_line(cmdline);                          // handles ; & built-ins execute
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
