#include "shell.h"

/*
 * History storage: a simple array of strings holding the last
 * HISTORY_SIZE commands. hist_buf[0] is the oldest command and
 * hist_buf[hist_count - 1] is the newest.
 */
static char* hist_buf[HISTORY_SIZE];
static int hist_count = 0;

/* Save a copy of cmdline. If the array is full, drop the oldest entry. */
void hist_add(const char* cmdline) {
    if (hist_count == HISTORY_SIZE) {
        free(hist_buf[0]);                       // free the oldest command
        for (int i = 1; i < HISTORY_SIZE; i++) { // shift everything left by one
            hist_buf[i - 1] = hist_buf[i];
        }
        hist_count--;
    }
    hist_buf[hist_count++] = strdup(cmdline);    // store our own copy
}

/* Print all stored commands with line numbers starting from 1. */
void hist_print(void) {
    for (int i = 0; i < hist_count; i++) {
        printf("%3d  %s\n", i + 1, hist_buf[i]);
    }
}

/* Return the n-th command (1-based), or NULL if n is out of range. */
char* hist_get(int n) {
    if (n < 1 || n > hist_count) {
        return NULL;
    }
    return hist_buf[n - 1];
}
