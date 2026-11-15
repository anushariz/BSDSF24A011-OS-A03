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
#define MAX_CMDS 10         // max commands joined by pipes in one segment
#define MAX_SEGS 32         // max ';' / '&' separated segments on one line
#define MAX_JOBS 32         // max background jobs tracked at once
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

/*
 * One segment of a command line, e.g. "sleep 5 &" in "echo hi ; sleep 5 &".
 * text points into the original line (the separators are overwritten by '\0').
 */
typedef struct {
    char* text;                // the command text of this segment
    int background;            // 1 if the segment ended with '&'
} segment_t;

// shell.c
char* read_cmd(char* prompt, FILE* fp);
int is_builtin(const char* name);
int handle_builtin(char** arglist);

// parser.c
char** tokenize(char* cmdline);
int parse_pipeline(char** tokens, command_t cmds[]);
int split_line(char* line, segment_t segs[], int max);

// execute.c
int execute_pipeline(command_t cmds[], int ncmds, int background, const char* text);
void run_builtin_in_shell(command_t* cmd);

// jobs.c
int jobs_add(pid_t pid, const char* cmd);
void jobs_reap(void);
void jobs_print(void);

// history.c
void hist_add(const char* cmdline);
void hist_print(void);
char* hist_get(int n);

// completion.c
void init_readline(void);

#endif // SHELL_H
