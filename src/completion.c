#include "shell.h"
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <readline/readline.h>
#include <readline/history.h>

/* Built-in commands offered when completing the first word of a line. */
static const char* builtins[] = { "exit", "cd", "help", "jobs", "history", "set", NULL };

/*
 * command_generator: called repeatedly by readline. state == 0 on the first
 * call for a new completion, then non-zero. Each call returns ONE matching
 * command name (malloc'd) or NULL when there are no more matches.
 * Matches come from the built-ins first, then executables found in PATH.
 */
static char* command_generator(const char* text, int state) {
    static int bi;                 // next built-in to check
    static char* path_copy = NULL; // modifiable copy of $PATH
    static char* saveptr = NULL;   // strtok_r position inside path_copy
    static int started;            // has strtok_r been started on path_copy?
    static DIR* dir = NULL;        // directory currently being scanned
    static char* cur_dir = NULL;   // name of that directory
    static size_t len;             // length of the text typed so far

    if (state == 0) {              // new completion: reset everything
        bi = 0;
        len = strlen(text);
        if (dir != NULL) { closedir(dir); dir = NULL; }
        free(path_copy);
        free(cur_dir);
        cur_dir = NULL;
        const char* path = getenv("PATH");
        path_copy = strdup(path != NULL ? path : "");
        saveptr = NULL;
        started = 0;
    }

    // 1. built-in commands
    while (builtins[bi] != NULL) {
        const char* name = builtins[bi++];
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }

    // 2. executable files in each directory of PATH
    for (;;) {
        if (dir == NULL) {
            char* d = strtok_r(started ? NULL : path_copy, ":", &saveptr);
            started = 1;
            if (d == NULL) break;                 // no more PATH directories
            free(cur_dir);
            cur_dir = strdup(d);
            dir = opendir(d);
            if (dir == NULL) continue;            // skip unreadable directory
        }
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strncmp(ent->d_name, text, len) != 0) continue;
            char full[PATH_MAX];
            snprintf(full, sizeof(full), "%s/%s", cur_dir, ent->d_name);
            struct stat st;
            if (stat(full, &st) == 0 && S_ISREG(st.st_mode) && access(full, X_OK) == 0) {
                return strdup(ent->d_name);
            }
        }
        closedir(dir);
        dir = NULL;
    }

    free(path_copy); path_copy = NULL;
    free(cur_dir);   cur_dir = NULL;
    return NULL;
}

/*
 * shell_completion: called by readline when Tab is pressed.
 * - First word on the line (and no '/' typed): complete a command name.
 * - Otherwise return NULL, so readline falls back to its default
 *   filename completion (files and directories in the current directory).
 */
static char** shell_completion(const char* text, int start, int end) {
    (void) end;
    for (int i = 0; i < start; i++) {
        if (rl_line_buffer[i] != ' ' && rl_line_buffer[i] != '\t') {
            return NULL;                       // not the first word
        }
    }
    if (strchr(text, '/') != NULL) {
        return NULL;                           // looks like a path: use filenames
    }
    return rl_completion_matches(text, command_generator);
}

/* Register our completion function with readline. */
void init_readline(void) {
    rl_attempted_completion_function = shell_completion;
}
