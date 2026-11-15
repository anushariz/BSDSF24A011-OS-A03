#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LEN 512
#define MAXARGS 64          // max tokens on one command line
#define ARGLEN 256
#define MAX_CMDS 10         // max commands joined by pipes on one line
#define HISTORY_SIZE 20
#define PROMPT "FCIT> "

/*
 * One simple command of a pipeline, e.g. "sort -r < in.txt > out.txt".
 * argv points into the token list made by tokenize(); it is NOT a copy.
 */
typedef struct {
    char* argv[MAXARGS + 1];   // NULL-terminated argument list
    char* infile;              // file after '<'  (NULL if none)
    char* outfile;             // file after '>'  (NULL if none)
} command_t;

// shell.c
char* read_cmd(char* prompt, FILE* fp);
int is_builtin(const char* name);
int handle_builtin(char** arglist);

// parser.c
char** tokenize(char* cmdline);
int parse_pipeline(char** tokens, command_t cmds[]);

// execute.c
int execute_pipeline(command_t cmds[], int ncmds);
void run_builtin_in_shell(command_t* cmd);

// history.c
void hist_add(const char* cmdline);
void hist_print(void);
char* hist_get(int n);

// completion.c
void init_readline(void);

#endif // SHELL_H
