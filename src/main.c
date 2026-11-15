#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {

        // Handle !n BEFORE tokenizing and BEFORE saving to history:
        // replace the "!n" line with the n-th command from history.
        if (cmdline[0] == '!') {
            char* end;
            long n = strtol(cmdline + 1, &end, 10);
            char* recalled = NULL;

            // valid only if digits followed the '!' and nothing else came after
            if (end != cmdline + 1 && *end == '\0') {
                recalled = hist_get((int) n);
            }

            if (recalled == NULL) {
                fprintf(stderr, "%s: event not found\n", cmdline);
                free(cmdline);
                continue;                      // back to the prompt
            }

            free(cmdline);
            cmdline = strdup(recalled);        // use a copy of the stored command
            printf("%s\n", cmdline);           // show what is being re-run
        }

        if ((arglist = tokenize(cmdline)) != NULL) {
            hist_add(cmdline);                 // store the command in history

            // Run external command only if it is not a built-in
            if (handle_builtin(arglist) == 0) {
                execute(arglist);
            }

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
