#include "shell.h"


char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace
        
        if (*cp == '\0') break; // Line was only whitespace

        start = cp;
        len = 1;
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
            len++;
        }
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for(int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}
// ---------------- Command history ----------------
static char* history[HISTORY_SIZE];   // last 20 commands
static int   hist_count = 0;          // how many are stored right now

void add_to_history(const char* cmd) {
    if (hist_count == HISTORY_SIZE) {             // full -> drop the OLDEST
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++)    // shift everything left
            history[i - 1] = history[i];
        hist_count--;
    }
    history[hist_count++] = strdup(cmd);          // store a private copy
}

void show_history(void) {
    for (int i = 0; i < hist_count; i++)
        printf("%d  %s\n", i + 1, history[i]);    // numbering starts from 1
}

char* get_history(int n) {
    if (n < 1 || n > hist_count)
        return NULL;                              // out of bounds
    return history[n - 1];
}

// ---------------- Input via GNU Readline ----------------
char* read_cmd(char* prompt, FILE* fp) {
    (void)fp;                           // readline() reads from the terminal itself
    char* cmdline = readline(prompt);   // prints prompt, handles editing, arrows, TAB
    if (cmdline == NULL)                // Ctrl+D (EOF)
        return NULL;
    if (cmdline[0] != '\0')             // only non-empty lines go to readline's history
        add_history(cmdline);           // makes UP/DOWN arrows work
    return cmdline;                     // malloc'd by readline -> main() free()s it
}
