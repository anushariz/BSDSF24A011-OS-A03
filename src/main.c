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

/*
 * run_line: execute one input line.
 * The line is split at ';' and '&' into segments, and the segments are run
 * one after the other. A segment ended by '&' runs in the background.
 */
static void run_line(char* line) {
    segment_t segs[MAX_SEGS];
    int nseg = split_line(line, segs, MAX_SEGS);
    if (nseg < 0) {
        return;                                // syntax error, already reported
    }

    for (int s = 0; s < nseg; s++) {
        char** tokens = tokenize(segs[s].text);
        if (tokens == NULL) {
            continue;                          // empty segment or bad quote
        }

        command_t cmds[MAX_CMDS];
        int ncmds = parse_pipeline(tokens, cmds);

        if (ncmds > 0) {
            if (ncmds == 1 && !segs[s].background && is_builtin(cmds[0].argv[0])) {
                run_builtin_in_shell(&cmds[0]);        // cd/exit must run in the shell itself
            } else {
                execute_pipeline(cmds, ncmds, segs[s].background, segs[s].text);
            }
        }

        // Free the memory allocated by tokenize()
        for (int i = 0; tokens[i] != NULL; i++) {
            free(tokens[i]);
        }
        free(tokens);

        if (ncmds < 0) {
            break;                             // syntax error: skip the rest of the line
        }
    }
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

        // Handle !n BEFORE splitting and BEFORE saving to history
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

        add_history(cmdline);                  // readline history (Up/Down arrows)
        hist_add(cmdline);                     // our history (history command, !n)

        run_line(cmdline);                     // note: this modifies cmdline in place
        free(cmdline);                         // readline's line is malloc'd
    }

    printf("\nShell exited.\n");
    return 0;
}
