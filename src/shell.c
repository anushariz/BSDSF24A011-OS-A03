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

/*
 * handle_builtin: checks if arglist[0] is a built-in command.
 * Built-ins must run inside the shell's own process (no fork), because
 * a child process cannot change the parent's directory or terminate it.
 * Returns 1 if the command was a built-in (and has been handled),
 * and 0 if it is not a built-in (the caller then uses fork-exec).
 */
int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL) {
        return 0;
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
        }
        return 1;
    }

    // help: list all built-in commands
    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit        - terminate the shell\n");
        printf("  cd <dir>    - change the current directory\n");
        printf("  help        - show this help message\n");
        printf("  jobs        - list background jobs (not yet implemented)\n");
        printf("  history     - show the last %d commands\n", HISTORY_SIZE);
        printf("  !n          - re-run the n-th command from history\n");
        return 1;
    }

    // jobs: placeholder for now
    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    // history: print stored commands with line numbers
    if (strcmp(arglist[0], "history") == 0) {
        hist_print();
        return 1;
    }

    return 0;   // not a built-in
}
