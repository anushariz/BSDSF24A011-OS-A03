#include "shell.h"

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }
    
    cmdline[pos] = '\0';
    return cmdline;
}

/* Names of all built-in commands. */
static const char* builtin_names[] = { "exit", "cd", "help", "jobs", "history", "set", NULL };

/* Returns 1 if name is a built-in command, 0 otherwise. */
int is_builtin(const char* name) {
    if (name == NULL) {
        return 0;
    }
    for (int i = 0; builtin_names[i] != NULL; i++) {
        if (strcmp(name, builtin_names[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * handle_builtin: checks if arglist[0] is a built-in command and runs it.
 * Built-ins must run inside the shell's own process (no fork), because
 * a child process cannot change the parent's directory or terminate it.
 * Returns -1 if it is NOT a built-in (the caller then uses fork-exec),
 * otherwise the built-in's exit status (0 = success, non-zero = failure).
 */
int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL) {
        return -1;
    }

    // exit: terminate the shell gracefully
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Shell exited.\n");
        exit(0);
    }

    // cd <directory>: change the shell's current working directory
    if (strcmp(arglist[0], "cd") == 0) {
        const char* target = arglist[1];
        if (target == NULL) {              // "cd" alone goes to HOME
            target = getenv("HOME");
            if (target == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return 1;
            }
        }
        if (chdir(target) != 0) {          // chdir() returns -1 on failure
            perror("cd failed");
            return 1;
        }
        return 0;
    }

    // help: list all built-in commands
    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit        - terminate the shell\n");
        printf("  cd <dir>    - change the current directory\n");
        printf("  help        - show this help message\n");
        printf("  jobs        - list running background jobs\n");
        printf("  history     - show the last %d commands\n", HISTORY_SIZE);
        printf("  !n          - re-run the n-th command from history\n");
        printf("  set         - show all shell variables\n");
        printf("Operators:\n");
        printf("  cmd > file  - send output to a file\n");
        printf("  cmd < file  - read input from a file\n");
        printf("  cmd1 | cmd2 - connect output of cmd1 to input of cmd2\n");
        printf("  cmd1 ; cmd2 - run cmd1, then cmd2\n");
        printf("  cmd &       - run cmd in the background\n");
        printf("Variables:\n");
        printf("  NAME=value  - define a variable (no spaces around '=')\n");
        printf("  $NAME       - use a variable's value in a command\n");
        printf("Control structure:\n");
        printf("  if cmd / then ... / else ... / fi\n");
        return 0;
    }

    // jobs: list the running background jobs
    if (strcmp(arglist[0], "jobs") == 0) {
        jobs_print();
        return 0;
    }

    // set: print all shell variables
    if (strcmp(arglist[0], "set") == 0) {
        var_print_all();
        return 0;
    }

    // history: print stored commands with line numbers
    if (strcmp(arglist[0], "history") == 0) {
        hist_print();
        return 0;
    }

    return -1;   // not a built-in
}
