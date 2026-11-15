#include "shell.h"
#include <readline/readline.h>

/* Parser states while reading an if-block. */
enum { ST_NEED_IF, ST_NEED_THEN, ST_THEN, ST_ELSE, ST_DONE };

/*
 * keyword_rest: if text starts with the word kw (followed by a blank or the
 * end of the text), return a pointer to whatever comes after it (blanks
 * skipped; "" if nothing). Otherwise return NULL.
 */
static char* keyword_rest(char* text, const char* kw) {
    size_t len = strlen(kw);
    if (strncmp(text, kw, len) != 0) {
        return NULL;
    }
    if (text[len] != '\0' && text[len] != ' ' && text[len] != '\t') {
        return NULL;
    }
    char* rest = text + len;
    while (*rest == ' ' || *rest == '\t') rest++;
    return rest;
}

/* Skip leading blanks. */
static char* skip_blanks(const char* s) {
    while (*s == ' ' || *s == '\t') s++;
    return (char*) s;
}

/* Does this input line start an if-block? */
int is_if_line(const char* line) {
    return keyword_rest(skip_blanks(line), "if") != NULL;
}

/* If the line starts with then/else/fi (outside any if-block), return that word. */
const char* stray_keyword(const char* line) {
    char* t = skip_blanks(line);
    if (keyword_rest(t, "then") != NULL) return "then";
    if (keyword_rest(t, "else") != NULL) return "else";
    if (keyword_rest(t, "fi") != NULL)   return "fi";
    return NULL;
}

/* Store a private copy of one command in a then/else list. */
static int add_cmd(segment_t list[], int* count, const char* text, int background) {
    if (keyword_rest((char*) text, "if") != NULL) {
        fprintf(stderr, "syntax error: nested 'if' is not supported\n");
        return -1;
    }
    if (*count >= MAX_BLOCK) {
        fprintf(stderr, "error: too many commands in one block\n");
        return -1;
    }
    list[*count].text = strdup(text);
    list[*count].background = background;
    (*count)++;
    return 0;
}

/*
 * feed_segment: advance the if-block state machine with ONE segment
 * (a piece of a line between ';' separators). Returns 0, or -1 on error.
 */
static int feed_segment(if_block_t* b, int* state, char* text, int background) {
    char* rest;

    switch (*state) {
    case ST_NEED_IF:                                   // first segment: "if <command>"
        rest = keyword_rest(text, "if");
        if (rest == NULL || *rest == '\0') {
            fprintf(stderr, "syntax error: expected a command after 'if'\n");
            return -1;
        }
        b->cond.text = strdup(rest);
        b->cond.background = background;
        *state = ST_NEED_THEN;
        return 0;

    case ST_NEED_THEN:                                 // next segment must be "then"
        rest = keyword_rest(text, "then");
        if (rest == NULL) {
            fprintf(stderr, "syntax error: expected 'then'\n");
            return -1;
        }
        *state = ST_THEN;
        if (*rest != '\0') {                           // "then <command>" on one line
            return add_cmd(b->then_cmds, &b->nthen, rest, background);
        }
        return 0;

    case ST_THEN:
    case ST_ELSE:
        if ((rest = keyword_rest(text, "fi")) != NULL) {
            if (*rest != '\0') {
                fprintf(stderr, "syntax error: unexpected text after 'fi'\n");
                return -1;
            }
            *state = ST_DONE;
            return 0;
        }
        if (*state == ST_THEN && (rest = keyword_rest(text, "else")) != NULL) {
            b->has_else = 1;
            *state = ST_ELSE;
            if (*rest != '\0') {                       // "else <command>" on one line
                return add_cmd(b->else_cmds, &b->nelse, rest, background);
            }
            return 0;
        }
        if (keyword_rest(text, "then") != NULL || keyword_rest(text, "else") != NULL) {
            fprintf(stderr, "syntax error: unexpected 'then' or 'else'\n");
            return -1;
        }
        if (*state == ST_THEN) {
            return add_cmd(b->then_cmds, &b->nthen, text, background);
        }
        return add_cmd(b->else_cmds, &b->nelse, text, background);
    }
    return -1;
}

/*
 * read_if_block: build an if_block_t that starts with first_line.
 * If 'fi' is not on that line, keep reading more lines with the "> " prompt
 * until the matching 'fi'. On success returns 0 and sets *full_text to a
 * malloc'd one-line version of the whole block (lines joined with " ; "),
 * which is what goes into the history. Returns -1 on a syntax error or EOF.
 */
int read_if_block(const char* first_line, if_block_t* blk, char** full_text) {
    memset(blk, 0, sizeof(*blk));
    int state = ST_NEED_IF;
    char* full = strdup(first_line);
    char* line = strdup(first_line);          // split_line() modifies its input

    for (;;) {
        segment_t segs[MAX_SEGS];
        int n = split_line(line, segs, MAX_SEGS);
        if (n < 0) goto fail;

        for (int i = 0; i < n; i++) {
            if (state == ST_DONE) {
                fprintf(stderr, "syntax error: unexpected command after 'fi'\n");
                goto fail;
            }
            if (feed_segment(blk, &state, segs[i].text, segs[i].background) < 0) {
                goto fail;
            }
        }
        free(line);
        line = NULL;

        if (state == ST_DONE) break;           // found the matching 'fi'

        line = readline("> ");                 // need another line of the block
        if (line == NULL) {                    // Ctrl+D
            fprintf(stderr, "syntax error: unexpected end of input (missing 'fi')\n");
            goto fail;
        }
        full = (char*) realloc(full, strlen(full) + strlen(line) + 4);
        strcat(full, " ; ");
        strcat(full, line);
    }

    *full_text = full;
    return 0;

fail:
    free(line);
    free(full);
    free_if_block(blk);
    return -1;
}

/*
 * execute_if: run the condition command in the normal way and look at its
 * exit status: 0 -> run the 'then' commands, non-zero -> run the 'else'
 * commands (if there are any).
 */
void execute_if(if_block_t* blk) {
    int status = run_segment(blk->cond.text, blk->cond.background);
    if (status < 0) {
        return;                                // syntax error in the condition
    }

    segment_t* list;
    int count;
    if (status == 0) {                         // success -> then part
        list = blk->then_cmds;
        count = blk->nthen;
    } else if (blk->has_else) {                // failure -> else part
        list = blk->else_cmds;
        count = blk->nelse;
    } else {
        return;                                // failure and no else part
    }

    for (int i = 0; i < count; i++) {
        if (run_segment(list[i].text, list[i].background) < 0) {
            break;                             // syntax error: stop this block
        }
    }
}

/* Free all copies stored in the block. */
void free_if_block(if_block_t* blk) {
    free(blk->cond.text);
    for (int i = 0; i < blk->nthen; i++) free(blk->then_cmds[i].text);
    for (int i = 0; i < blk->nelse; i++) free(blk->else_cmds[i].text);
    memset(blk, 0, sizeof(*blk));
}
