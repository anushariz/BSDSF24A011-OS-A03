#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

int main() {
    char* cmdline;
    char** arglist;

    init_readline();                           // set up Tab completion

    // readline() shows the prompt, handles line editing, arrow keys and Tab
    while ((cmdline = readline(PROMPT)) != NULL) {

        // Handle !n BEFORE tokenizing and BEFORE saving to history
        if (cmdline[0] == '!') {
            char* end;
            long n = strtol(cmdline + 1, &end, 10);
            char* recalled = NULL;

            if (end != cmdline + 1 && *end == '\0') {
                recalled = hist_get((int) n);
            }

            if (recalled == NULL) {
                fprintf(stderr, "%s: event not found\n", cmdline);
                free(cmdline);
                continue;
            }

            free(cmdline);
            cmdline = strdup(recalled);
            printf("%s\n", cmdline);
        }

        if ((arglist = tokenize(cmdline)) != NULL) {
            add_history(cmdline);              // readline history (Up/Down arrows)
            hist_add(cmdline);                 // our history (history command, !n)

            if (handle_builtin(arglist) == 0) {
                execute(arglist);
            }

            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);                         // readline's line is malloc'd
    }

    printf("\nShell exited.\n");
    return 0;
}
