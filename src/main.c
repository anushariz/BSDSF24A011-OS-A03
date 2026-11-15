#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

/* Returns 1 if the string contains only spaces/tabs (or is empty). */
static int is_blank(const char* s) {
    for (; *s != '\0'; s++) {
        if (*s != ' ' && *s != '\t') return 0;
    }
    return 1;
}

int main() {
    char* cmdline;

    init_readline();                           // set up Tab completion

    for (;;) {
        // Before showing the prompt: collect finished background jobs
        // (waitpid with WNOHANG) so no zombie processes are left behind.
        jobs_reap();

        // readline() shows the prompt, handles line editing, arrow keys and Tab
        cmdline = readline(PROMPT);
        if (cmdline == NULL) {
            break;                             // Ctrl+D
        }

        // Handle !n BEFORE anything else and BEFORE saving to history
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

        if (is_blank(cmdline)) {               // nothing to run
            free(cmdline);
            continue;
        }

        // A line that starts with "if" begins a multi-line if-then-else-fi block
        if (is_if_line(cmdline)) {
            if_block_t blk;
            char* full;
            if (read_if_block(cmdline, &blk, &full) == 0) {
                add_history(full);             // the whole block as ONE history entry
                hist_add(full);
                free(full);
                execute_if(&blk);
                free_if_block(&blk);
            }
            free(cmdline);
            continue;
        }

        // then/else/fi without a matching if
        const char* stray = stray_keyword(cmdline);
        if (stray != NULL) {
            fprintf(stderr, "syntax error: unexpected '%s'\n", stray);
            free(cmdline);
            continue;
        }

        add_history(cmdline);                  // readline history (Up/Down arrows)
        hist_add(cmdline);                     // our history (history command, !n)

        run_line(cmdline);                     // note: this modifies cmdline in place
        free(cmdline);                         // readline's line is malloc'd
    }

    printf("\nShell exited.\n");
    return 0;
}
