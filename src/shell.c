#include "shell.h"


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

// ---------------- Tokenizer with operators and quotes ----------------
#define IS_OP(c) ((c) == '<' || (c) == '>' || (c) == '|' || (c) == '&')

char** tokenize(char* cmdline) {
    if (cmdline == NULL || cmdline[0] == '\0')
        return NULL;

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    char* buf = (char*)malloc(strlen(cmdline) + 1);   // scratch space for one token
    int argnum = 0;
    char* cp = cmdline;

    while (*cp != '\0') {
        while (*cp == ' ' || *cp == '\t') cp++;       // skip whitespace
        if (*cp == '\0') break;

        if (argnum >= MAXARGS) {
            fprintf(stderr, "myshell: too many arguments (max %d)\n", MAXARGS);
            break;
        }

        int len = 0;
        if (IS_OP(*cp)) {
            buf[len++] = *cp++;                        // < > | is a token by itself
        } else {
            char quote = 0;                            // 0 = not inside quotes
            while (*cp != '\0' && (quote || !(*cp == ' ' || *cp == '\t' || IS_OP(*cp)))) {
                if (quote) {
                    if (*cp == quote) quote = 0;       // closing quote (not copied)
                    else buf[len++] = *cp;             // inside quotes: copy everything
                } else if (*cp == '"' || *cp == '\'') {
                    quote = *cp;                       // opening quote (not copied)
                } else {
                    buf[len++] = *cp;
                }
                cp++;
            }
        }
        buf[len] = '\0';
        arglist[argnum++] = strdup(buf);               // each token gets its own heap copy
    }
    free(buf);

    if (argnum == 0) {
        free(arglist);
        return NULL;
    }
    arglist[argnum] = NULL;
    return arglist;
}

// ---------------- Command chaining (;) and background (&) ----------------

// If the LAST token is "&", remove it and return 1 (background). Else 0.
static int strip_ampersand(char** arglist) {
    int last = 0;
    while (arglist[last] != NULL) last++;
    if (last > 0 && strcmp(arglist[last - 1], "&") == 0) {
        free(arglist[last - 1]);
        arglist[last - 1] = NULL;
        return 1;
    }
    return 0;
}

// Runs ONE command (no ';' inside). Returns its exit status.
static int run_single(char* cmd) {
    char** arglist = tokenize(cmd);
    if (arglist == NULL)
        return 0;

    int background = strip_ampersand(arglist);
    int status = 0;

    if (arglist[0] == NULL) {
        fprintf(stderr, "myshell: syntax error near '&'\n");
        status = 1;
    } else {
        if (handle_builtin(arglist) == 0)
            status = execute(arglist, background);
    }

    for (int i = 0; arglist[i] != NULL; i++)
        free(arglist[i]);
    free(arglist);
    return status;
}

// Splits the line at every ';' NOT inside quotes, runs pieces in order.
int run_line(char* line) {
    int status = 0;
    char* start = line;
    char quote = 0;

    for (char* p = line; ; p++) {
        if (*p == '"' || *p == '\'') {
            if (quote == 0) quote = *p;
            else if (quote == *p) quote = 0;
        }
        if ((*p == ';' && quote == 0) || *p == '\0') {
            char saved = *p;
            *p = '\0';
            status = run_single(start);
            if (saved == '\0') break;
            start = p + 1;
        }
    }
    return status;
}
