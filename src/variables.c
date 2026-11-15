#include "shell.h"
#include <ctype.h>

/*
 * Shell variables are stored in a singly linked list of name/value pairs.
 * New variables are added at the END of the list, so 'set' shows them in
 * the order they were first defined.
 */
typedef struct var {
    char* name;
    char* value;
    struct var* next;
} var_t;

static var_t* var_head = NULL;

/* Find a variable by name. Returns NULL if it is not defined. */
static var_t* var_find(const char* name) {
    for (var_t* v = var_head; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            return v;
        }
    }
    return NULL;
}

/* Create the variable, or change its value if it already exists. */
static void var_set(const char* name, const char* value) {
    var_t* v = var_find(name);
    if (v != NULL) {                          // already defined: replace the value
        free(v->value);
        v->value = strdup(value);
        return;
    }

    v = (var_t*) malloc(sizeof(var_t));       // new variable
    v->name = strdup(name);
    v->value = strdup(value);
    v->next = NULL;

    if (var_head == NULL) {                   // first variable
        var_head = v;
    } else {                                  // append at the end
        var_t* last = var_head;
        while (last->next != NULL) last = last->next;
        last->next = v;
    }
}

/* The 'set' built-in: print every variable as NAME=value. */
void var_print_all(void) {
    if (var_head == NULL) {
        printf("No variables defined.\n");
        return;
    }
    for (var_t* v = var_head; v != NULL; v = v->next) {
        printf("%s=%s\n", v->name, v->value);
    }
}

/* A variable name starts with a letter or '_' and continues with letters, digits or '_'. */
static int is_name_start(char c) {
    return isalpha((unsigned char) c) || c == '_';
}
static int is_name_char(char c) {
    return isalnum((unsigned char) c) || c == '_';
}

/*
 * If the token looks like NAME=value (valid NAME, then '='), return a pointer
 * to the '='. Otherwise return NULL.
 */
static const char* assignment_eq(const char* token) {
    if (!is_name_start(token[0])) {
        return NULL;
    }
    const char* p = token + 1;
    while (is_name_char(*p)) p++;
    return (*p == '=') ? p : NULL;
}

/* Append n bytes of s to a growing buffer (it is enlarged when needed). */
static void buf_append(char** buf, size_t* len, size_t* cap, const char* s, size_t n) {
    while (*len + n + 1 > *cap) {
        *cap *= 2;
        *buf = (char*) realloc(*buf, *cap);
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = '\0';
}

/*
 * expand_token: return a NEW malloc'd string where every $NAME (or ${NAME})
 * in 'token' is replaced by the variable's value. A variable that is not
 * defined as a shell variable falls back to the environment (so $HOME works)
 * and otherwise becomes an empty string. A '$' that is not followed by a
 * valid name (like "$5" or a lone "$") is left unchanged.
 */
static char* expand_token(const char* token) {
    size_t cap = strlen(token) + 16;
    size_t len = 0;
    char* out = (char*) malloc(cap);
    out[0] = '\0';

    const char* p = token;
    while (*p != '\0') {
        if (*p == '$' && (is_name_start(p[1]) || (p[1] == '{' && is_name_start(p[2])))) {
            const char* start;
            const char* end;
            const char* after;

            if (p[1] == '{') {                         // ${NAME}
                start = p + 2;
                end = start;
                while (is_name_char(*end)) end++;
                if (*end != '}') {                     // no closing brace: keep as text
                    buf_append(&out, &len, &cap, p, 1);
                    p++;
                    continue;
                }
                after = end + 1;
            } else {                                   // $NAME
                start = p + 1;
                end = start;
                while (is_name_char(*end)) end++;
                after = end;
            }

            char* name = strndup(start, end - start);
            var_t* v = var_find(name);
            const char* value = (v != NULL) ? v->value : getenv(name);
            if (value == NULL) value = "";             // undefined -> empty string
            buf_append(&out, &len, &cap, value, strlen(value));
            free(name);
            p = after;
        } else {
            buf_append(&out, &len, &cap, p, 1);        // ordinary character
            p++;
        }
    }
    return out;
}

/*
 * expand_variables: the pre-execution expansion step. Called after
 * tokenize() and before the command runs: every token that contains a '$'
 * is replaced by its expanded copy (the old string is freed).
 */
void expand_variables(char** tokens) {
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strchr(tokens[i], '$') != NULL) {
            char* expanded = expand_token(tokens[i]);
            free(tokens[i]);
            tokens[i] = expanded;
        }
    }
}

/*
 * is_assignment_line: 1 if EVERY token is of the form NAME=value
 * (e.g. "MSG=hello" or "A=1 B=2"). "MSG = hello" is not an assignment,
 * because it has spaces around the '='.
 */
int is_assignment_line(char** tokens) {
    if (tokens[0] == NULL) {
        return 0;
    }
    for (int i = 0; tokens[i] != NULL; i++) {
        if (assignment_eq(tokens[i]) == NULL) {
            return 0;
        }
    }
    return 1;
}

/*
 * do_assignments: the built-in action for NAME=value. It runs inside the
 * shell process (no fork), because a child could not change the shell's
 * variables. $VAR inside the value is expanded first, so B=$A works.
 */
void do_assignments(char** tokens) {
    for (int i = 0; tokens[i] != NULL; i++) {
        const char* eq = assignment_eq(tokens[i]);
        char* name = strndup(tokens[i], eq - tokens[i]);
        char* value = expand_token(eq + 1);
        var_set(name, value);
        free(name);
        free(value);
    }
}
