#include "shell.h"

/*
 * run_segment: run ONE segment of a line, e.g. "cat f | sort > out".
 * (tokenize -> assignment or expansion -> parse_pipeline -> built-in or
 * fork-exec-wait)
 * Returns the exit status of the command (0 = success), or -1 if the
 * segment had a syntax error and nothing was run.
 */
int run_segment(char* text, int background) {
    char** tokens = tokenize(text);
    if (tokens == NULL) {
        return -1;                             // empty segment or bad quote
    }

    int status = -1;

    if (is_assignment_line(tokens)) {
        do_assignments(tokens);                // NAME=value: stored by the shell itself
        status = 0;
    } else {
        expand_variables(tokens);              // replace $NAME by its value

        command_t cmds[MAX_CMDS];
        int ncmds = parse_pipeline(tokens, cmds);

        if (ncmds > 0) {
            if (ncmds == 1 && !background && is_builtin(cmds[0].argv[0])) {
                status = run_builtin_in_shell(&cmds[0]);   // cd/exit must run in the shell itself
            } else {
                status = execute_pipeline(cmds, ncmds, background, text);
            }
        }
    }

    // Free the memory allocated by tokenize()
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    free(tokens);
    return status;
}

/*
 * run_line: execute one input line.
 * The line is split at ';' and '&' into segments, and the segments are run
 * one after the other. A segment ended by '&' runs in the background.
 * (This modifies 'line' in place.)
 */
void run_line(char* line) {
    segment_t segs[MAX_SEGS];
    int nseg = split_line(line, segs, MAX_SEGS);
    if (nseg < 0) {
        return;                                // syntax error, already reported
    }

    for (int s = 0; s < nseg; s++) {
        if (run_segment(segs[s].text, segs[s].background) < 0) {
            break;                             // syntax error: skip the rest of the line
        }
    }
}
