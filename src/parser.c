#include "shell.h"

/* Is this character one of the special operator characters? */
static int is_special(char c) {
    return c == '<' || c == '>' || c == '|';
}

/* Is this whole token an operator? */
static int is_operator(const char* t) {
    return strcmp(t, "<") == 0 || strcmp(t, ">") == 0 || strcmp(t, "|") == 0;
}

/*
 * tokenize: split a command line into tokens.
 * - Words are separated by spaces/tabs.
 * - The characters < > | are always one-character tokens of their own,
 *   even with no spaces around them: "ls>out.txt" -> ls  >  out.txt
 * - Text inside single or double quotes is kept together as part of one
 *   word (spaces and operators lose their special meaning) and the quote
 *   characters themselves are removed: grep "my name" -> grep, my name
 * Returns a NULL-terminated array of malloc'd strings (caller frees them),
 * or NULL for an empty line or an unterminated quote.
 */
char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**) malloc(sizeof(char*) * (MAXARGS + 1));
    char* cp = cmdline;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;    // skip whitespace
        if (*cp == '\0') break;                    // line was only whitespace

        if (is_special(*cp)) {                     // operator: a token by itself
            arglist[argnum++] = strndup(cp, 1);
            cp++;
            continue;
        }

        // ordinary word (may contain quoted parts)
        char* word = (char*) malloc(strlen(cp) + 1);
        int len = 0;
        char quote = 0;                            // 0 = not inside quotes
        while (*cp != '\0') {
            if (quote != 0) {
                if (*cp == quote) {
                    quote = 0;                     // closing quote
                } else {
                    word[len++] = *cp;             // keep everything inside quotes
                }
            } else if (*cp == '"' || *cp == '\'') {
                quote = *cp;                       // opening quote
            } else if (*cp == ' ' || *cp == '\t' || is_special(*cp)) {
                break;                             // end of this word
            } else {
                word[len++] = *cp;
            }
            cp++;
        }

        if (quote != 0) {                          // quote was never closed
            fprintf(stderr, "syntax error: unterminated quote\n");
            free(word);
            for (int i = 0; i < argnum; i++) free(arglist[i]);
            free(arglist);
            return NULL;
        }
        word[len] = '\0';
        arglist[argnum++] = word;
    }

    if (argnum == 0) {                             // nothing was parsed
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

/*
 * parse_pipeline: turn the token list into an array of commands.
 * Tokens are split at '|'. Inside one command, "< file" and "> file"
 * are stored in infile/outfile and everything else goes into argv.
 * Returns the number of commands, or -1 on a syntax error.
 */
int parse_pipeline(char** tokens, command_t cmds[]) {
    int n = 0;          // index of the command being built
    int argc = 0;       // number of arguments stored in cmds[n].argv
    memset(&cmds[0], 0, sizeof(command_t));

    for (int i = 0; tokens[i] != NULL; i++) {
        char* t = tokens[i];

        if (strcmp(t, "|") == 0) {                 // end of this command
            if (argc == 0) {
                fprintf(stderr, "syntax error: missing command before '|'\n");
                return -1;
            }
            cmds[n].argv[argc] = NULL;
            n++;
            if (n >= MAX_CMDS) {
                fprintf(stderr, "error: too many commands in one pipeline\n");
                return -1;
            }
            memset(&cmds[n], 0, sizeof(command_t));
            argc = 0;
        }
        else if (strcmp(t, "<") == 0 || strcmp(t, ">") == 0) {
            char* file = tokens[i + 1];            // the next token must be a file name
            if (file == NULL || is_operator(file)) {
                fprintf(stderr, "syntax error: expected a file name after '%s'\n", t);
                return -1;
            }
            if (t[0] == '<') {
                cmds[n].infile = file;
            } else {
                cmds[n].outfile = file;
            }
            i++;                                   // skip the file name token
        }
        else {
            cmds[n].argv[argc++] = t;              // ordinary argument
        }
    }

    if (argc == 0) {                               // e.g. "ls |" or just "> file"
        fprintf(stderr, "syntax error: missing command\n");
        return -1;
    }
    cmds[n].argv[argc] = NULL;
    return n + 1;
}
