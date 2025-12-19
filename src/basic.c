// ...existing code...
/*
  Tiny BASIC interpreter (single-file)
  - Floating point variables A..Z, A0..Z9
  - Arrays via DIM A(n)
  - Statements: LET, PRINT, GOTO, IF ... THEN <lineno>, FOR/NEXT, END
  - CLI commands: LOAD <file>, SAVE <file>, LIST, RUN, NEW, QUIT
  Build: gcc -std=c99 -O2 -o basic basic.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string.h>

#define MAX_LINES 1000
#define MAX_LINE_LEN 512
#define MAX_FOR_DEPTH 256

typedef struct {
    int lineno;
    char *text;
} Line;

static Line program[MAX_LINES];
static int program_count = 0;

/* scalar variables A..Z and A0..Z9 */
#define NUM_VARS 286
static double vars[NUM_VARS];
/* arrays for A..Z */
static double *arrays[26];
static int arrays_size[26];

/* FOR stack frame */
typedef struct {
    int var;        /* 0..25 */
    double end;
    double step;
    int for_pc;     /* program index of FOR statement */
} ForFrame;
static ForFrame for_stack[MAX_FOR_DEPTH];
static int for_sp = 0;

/* current program counter index (set by run_program before calling run_statement) */
static int current_pc = 0;

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s) - 1;
    while (e >= s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

static int parse_var_name(const char **s) {
    if (!isalpha((unsigned char)**s)) return -1;
    char letter = toupper((unsigned char)**s);
    (*s)++;
    int base = letter - 'A';
    if (isdigit((unsigned char)**s)) {
        int digit = **s - '0';
        (*s)++;
        return 26 + base * 10 + digit;
    } else {
        return base;
    }
}

static int find_index_by_lineno(int ln) {
    for (int i = 0; i < program_count; ++i) if (program[i].lineno == ln) return i;
    return -1;
}

static void sort_program() {
    for (int i = 0; i < program_count; ++i)
        for (int j = i + 1; j < program_count; ++j)
            if (program[i].lineno > program[j].lineno) {
                Line t = program[i]; program[i] = program[j]; program[j] = t;
            }
}

static void insert_or_replace_line(int lineno, const char *text) {
    int idx = find_index_by_lineno(lineno);
    if (idx >= 0) {
        free(program[idx].text);
        program[idx].text = strdup(text);
        return;
    }
    if (program_count >= MAX_LINES) { fprintf(stderr,"Program full\n"); return; }
    program[program_count].lineno = lineno;
    program[program_count].text = strdup(text);
    program_count++;
    sort_program();
}

static void delete_line(int lineno) {
    int idx = find_index_by_lineno(lineno);
    if (idx < 0) return;
    free(program[idx].text);
    for (int i = idx; i + 1 < program_count; ++i) program[i] = program[i+1];
    program_count--;
}

static void list_program(FILE *out) {
    for (int i = 0; i < program_count; ++i) {
        fprintf(out, "%d %s\n", program[i].lineno, program[i].text);
    }
}

static int load_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[MAX_LINE_LEN];
    /* clear existing program */
    for (int i = 0; i < program_count; ++i) free(program[i].text);
    program_count = 0;
    while (fgets(buf, sizeof(buf), f)) {
        char *p = trim(buf);
        if (*p == '\0') continue;
        char *sp = strchr(p, ' ');
        if (sp) {
            *sp = '\0';
            int ln = atoi(p);
            char *rest = trim(sp + 1);
            insert_or_replace_line(ln, rest);
        } else {
            int ln = atoi(p);
            delete_line(ln);
        }
    }
    fclose(f);
    return 1;
}

static int save_file(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    for (int i = 0; i < program_count; ++i) {
        fprintf(f, "%d %s\n", program[i].lineno, program[i].text);
    }
    fclose(f);
    return 1;
}

/* Expression parser (recursive descent)
   Supports + - * / parentheses, numbers, variables, array access like A(expr)
*/
typedef const char *pc;

static void skip_spaces(pc *s) {
    while (**s && isspace((unsigned char)**s)) (*s)++;
}

static double parse_number(pc *s) {
    skip_spaces(s);
    char *end;
    double v = strtod(*s, &end);
    *s = end;
    return v;
}

static double parse_expr(pc *s);

static double parse_factor(pc *s) {
    skip_spaces(s);
    if (**s == '+' || **s == '-') {
        int sign = (**s == '-') ? -1 : 1;
        (*s)++;
        return sign * parse_factor(s);
    } else if (**s == '(') {
        (*s)++;
        double v = parse_expr(s);
        skip_spaces(s);
        if (**s == ')') (*s)++;
        return v;
    } else if (isalpha((unsigned char)**s)) {
        int var_index = parse_var_name(s);
        if (var_index == -1) return 0.0;
        skip_spaces(s);
        if (**s == '(') {
            /* array access, only for A-Z */
            if (var_index >= 26) {
                fprintf(stderr,"Runtime error: arrays only supported for A-Z\n");
                return 0.0;
            }
            (*s)++; /* consume '(' */
            int idx = (int)parse_expr(s);
            skip_spaces(s);
            if (**s == ')') (*s)++;
            if (idx < 0) return 0.0;
            int id = var_index;
            if (!arrays[id]) {
                fprintf(stderr,"Runtime error: array %c not DIM'd\n", 'A' + id);
                return 0.0;
            }
            if (idx >= arrays_size[id]) {
                fprintf(stderr,"Runtime error: array %c index %d out of bounds\n", 'A' + id, idx);
                return 0.0;
            }
            return arrays[id][idx];
        } else {
            return vars[var_index];
        }
    } else {
        return parse_number(s);
    }
}

static double parse_term(pc *s) {
    double v = parse_factor(s);
    while (1) {
        skip_spaces(s);
        if (**s == '*') { (*s)++; double r = parse_factor(s); v = v * r; }
        else if (**s == '/') { (*s)++; double r = parse_factor(s); if (r==0.0) r = 1.0; v = v / r; }
        else break;
    }
    return v;
}

static double parse_expr(pc *s) {
    double v = parse_term(s);
    while (1) {
        skip_spaces(s);
        if (**s == '+') { (*s)++; double r = parse_term(s); v = v + r; }
        else if (**s == '-') { (*s)++; double r = parse_term(s); v = v - r; }
        else break;
    }
    return v;
}

/* Evaluate relational condition, returns 1 or 0 and advances pointer */
static int eval_condition(pc *s) {
    skip_spaces(s);
    double left = parse_expr(s);
    skip_spaces(s);
    if (strncmp(*s, "<=", 2) == 0) { (*s)+=2; double r = parse_expr(s); return left <= r; }
    if (strncmp(*s, ">=", 2) == 0) { (*s)+=2; double r = parse_expr(s); return left >= r; }
    if (strncmp(*s, "<>", 2) == 0) { (*s)+=2; double r = parse_expr(s); return left != r; }
    if (**s == '<') { (*s)++; double r = parse_expr(s); return left < r; }
    if (**s == '>') { (*s)++; double r = parse_expr(s); return left > r; }
    if (**s == '=' || strncmp(*s, "==", 2) == 0) {
        if (**s == '=') (*s)++; else (*s)+=2;
        double r = parse_expr(s); return left == r;
    }
    return left != 0.0;
}

static int find_matching_next(int start_pc) {
    int count = 1; // starting after FOR
    for (int i = start_pc; i < program_count; ++i) {
        const char *text = program[i].text;
        pc s = text;
        skip_spaces(&s);
        if (strncasecmp(s, "FOR", 3) == 0 && isspace((unsigned char)s[3])) {
            count++;
        } else if (strncasecmp(s, "NEXT", 4) == 0 && (s[4] == ' ' || s[4] == '\0')) {
            count--;
            if (count == 0) {
                return i;
            }
        }
    }
    return -1;
}

/* Execute a single statement. Returns:
   1 = continue to next line
   2 = jump performed (next_index set)
   0 = END / stop
  -1 = runtime error
*/
static int run_statement(const char *text, int *next_index) {
    pc s = text;
    skip_spaces(&s);
    /* LET */
    if (strncasecmp(s, "LET", 3) == 0 && isspace((unsigned char)s[3])) {
        s += 3; skip_spaces(&s);
        if (isalpha((unsigned char)*s)) {
            int var_index = parse_var_name(&s);
            if (var_index == -1) return -1;
            skip_spaces(&s);
            if (*s == '(') {
                /* array assign, only for A-Z */
                if (var_index >= 26) {
                    fprintf(stderr,"Runtime error: arrays only supported for A-Z\n");
                    return -1;
                }
                s++; int idx = (int)parse_expr(&s); skip_spaces(&s); if (*s == ')') s++;
                skip_spaces(&s); if (*s == '=') s++;
                double val = parse_expr(&s);
                int id = var_index;
                if (!arrays[id]) { fprintf(stderr,"Runtime error: array %c not DIM'd\n", 'A' + id); return -1; }
                if (idx < 0 || idx >= arrays_size[id]) { fprintf(stderr,"Runtime error: array %c index %d out of bounds\n", 'A' + id, idx); return -1; }
                arrays[id][idx] = val;
                return 1;
            } else {
                if (*s == '=') s++;
                double val = parse_expr(&s);
                vars[var_index] = val;
                return 1;
            }
        }
        return 1;
    }
    /* DIM */
    if (strncasecmp(s, "DIM", 3) == 0 && isspace((unsigned char)s[3])) {
        s += 3; skip_spaces(&s);
        while (1) {
            if (!isalpha((unsigned char)*s)) break;
            char name = toupper((unsigned char)*s); s++;
            skip_spaces(&s);
            if (*s != '(') { fprintf(stderr,"Syntax error in DIM\n"); return -1; }
            s++;
            int size = (int)parse_expr(&s);
            if (*s == ')') s++;
            if (size < 0) size = 0;
            int id = name - 'A';
            free(arrays[id]);
            arrays_size[id] = size + 1; /* allow 0..size */
            arrays[id] = calloc((size_t)arrays_size[id], sizeof(double));
            skip_spaces(&s);
            if (*s == ',') { s++; skip_spaces(&s); continue; }
            break;
        }
        return 1;
    }

    /* FOR */
    if (strncasecmp(s, "FOR", 3) == 0 && isspace((unsigned char)s[3])) {
        s += 3; skip_spaces(&s);
        if (!isalpha((unsigned char)*s)) { fprintf(stderr,"Syntax error in FOR\n"); return -1; }
        int var_index = parse_var_name(&s);
        if (var_index == -1) { fprintf(stderr,"Syntax error in FOR\n"); return -1; }
        skip_spaces(&s);
        if (*s == '=') s++;
        double start = parse_expr(&s);
        skip_spaces(&s);
        if (strncasecmp(s, "TO", 2) != 0) { fprintf(stderr,"Syntax error in FOR: missing TO\n"); return -1; }
        s += 2; skip_spaces(&s);
        double end = parse_expr(&s);
        skip_spaces(&s);
        double step = 1.0;
        if (strncasecmp(s, "STEP", 4) == 0 && (isspace((unsigned char)s[4]) || s[4] == '\0')) {
            s += 4; skip_spaces(&s);
            step = parse_expr(&s);
            if (step == 0.0) step = 1.0;
        }
        vars[var_index] = start;
        if ((step > 0.0 && start > end) || (step < 0.0 && start < end)) {
            int next_line = find_matching_next(current_pc + 1);
            if (next_line >= 0) {
                *next_index = next_line + 1;
                return 2;
            } else {
                fprintf(stderr,"Runtime error: FOR without matching NEXT\n");
                return -1;
            }
        } else {
            if (for_sp >= MAX_FOR_DEPTH) { fprintf(stderr,"Runtime error: FOR stack overflow\n"); return -1; }
            for_stack[for_sp].var = var_index;
            for_stack[for_sp].end = end;
            for_stack[for_sp].step = step;
            for_stack[for_sp].for_pc = current_pc;
            for_sp++;
            return 1;
        }
    }

    /* PRINT */
    if (strncasecmp(s, "PRINT", 5) == 0 && (s[5] == ' ' || s[5] == '\0')) {
        s += 5; skip_spaces(&s);
        if (*s == '"' || *s == '\'') {
            char q = *s++; while (*s && *s != q) putchar(*s++); putchar('\n'); return 1;
        } else {
            double v = parse_expr(&s);
            printf("%g\n", v);
            return 1;
        }
    }
    /* GOTO */
    if (strncasecmp(s, "GOTO", 4) == 0 && isspace((unsigned char)s[4])) {
        s += 4; skip_spaces(&s);
        int ln = atoi(s);
        int idx = find_index_by_lineno(ln);
        if (idx < 0) { fprintf(stderr,"Runtime error: GOTO to %d not found\n", ln); return -1; }
        *next_index = idx;
        return 2;
    }
    /* IF ... THEN <lineno> */
    if (strncasecmp(s, "IF", 2) == 0 && isspace((unsigned char)s[2])) {
        s += 2; skip_spaces(&s);
        int cond = eval_condition(&s);
        skip_spaces(&s);
        if (strncasecmp(s, "THEN", 4) == 0) {
            s += 4; skip_spaces(&s);
            int ln = atoi(s);
            if (cond) {
                int idx = find_index_by_lineno(ln);
                if (idx < 0) { fprintf(stderr,"Runtime error: THEN to %d not found\n", ln); return -1; }
                *next_index = idx;
                return 2;
            }
            return 1;
        }
        return 1;
    }
    /* END */
    if (strncasecmp(s, "END", 3) == 0) {
        return 0;
    }

    /* NEXT */
    if (strncasecmp(s, "NEXT", 4) == 0 && (s[4] == ' ' || s[4] == '\0')) {
        s += 4; skip_spaces(&s);
        int var_index = -1;
        if (isalpha((unsigned char)*s)) {
            var_index = parse_var_name(&s);
        }
        if (for_sp == 0) { fprintf(stderr,"Runtime error: NEXT without FOR\n"); return -1; }
        int frame_index = for_sp - 1;
        if (var_index != -1) {
            /* require the top frame to match the variable */
            if (for_stack[frame_index].var != var_index) {
                fprintf(stderr,"Runtime error: NEXT variable does not match FOR\n");
                return -1;
            }
        }
        /* operate on top frame */
        ForFrame *f = &for_stack[frame_index];
        vars[f->var] += f->step;
        /* check continuation */
        if ((f->step > 0.0 && vars[f->var] <= f->end) || (f->step < 0.0 && vars[f->var] >= f->end)) {
            /* jump back to line after FOR */
            *next_index = f->for_pc + 1;
            return 2;
        } else {
            /* pop frame and continue */
            for_sp--;
            return 1;
        }
    }

    /* allow short assignment: A = expr or A(i) = expr */
    if (isalpha((unsigned char)*s)) {
        int var_index = parse_var_name(&s);
        if (var_index == -1) return -1;
        skip_spaces(&s);
        if (*s == '(') {
            if (var_index >= 26) {
                fprintf(stderr,"Runtime error: arrays only supported for A-Z\n");
                return -1;
            }
            s++; int idx = (int)parse_expr(&s); if (*s == ')') s++;
            skip_spaces(&s);
            if (*s == '=') { s++; double val = parse_expr(&s);
                int id = var_index;
                if (!arrays[id]) { fprintf(stderr,"Runtime error: array %c not DIM'd\n", 'A' + id); return -1; }
                if (idx < 0 || idx >= arrays_size[id]) { fprintf(stderr,"Runtime error: array %c index %d out of bounds\n", 'A' + id, idx); return -1; }
                arrays[id][idx] = val;
                return 1;
            }
        } else if (*s == '=') {
            s++; double val = parse_expr(&s);
            vars[var_index] = val;
            return 1;
        }
    }
    fprintf(stderr,"Unknown statement: %s\n", text);
    return -1;
}

static void run_program(void) {
    /* reset variables, arrays and FOR stack */
    for (int i = 0; i < NUM_VARS; ++i) vars[i] = 0;
    for (int i = 0; i < 26; ++i) { free(arrays[i]); arrays[i] = NULL; arrays_size[i] = 0; }
    for_sp = 0;

    if (program_count == 0) return;
    int pc = 0;
    while (pc >= 0 && pc < program_count) {
        current_pc = pc;
        const char *stmt = program[pc].text;
        int next = pc + 1;
        int r = run_statement(stmt, &next);
        if (r == 0) break;
        if (r == -1) break;
        if (r == 2) pc = next;
        else pc = next;
    }
}

/* CLI helpers */
static void do_new(void) {
    for (int i = 0; i < program_count; ++i) free(program[i].text);
    program_count = 0;
    for (int i = 0; i < NUM_VARS; ++i) vars[i] = 0;
    for (int i = 0; i < 26; ++i) { free(arrays[i]); arrays[i] = NULL; arrays_size[i] = 0; }
    for_sp = 0;
}

static void strip_nl(char *s) {
    char *p = strchr(s, '\n'); if (p) *p = '\0';
    p = strchr(s, '\r'); if (p) *p = '\0';
}

int main(int argc, char **argv) {
    char line[MAX_LINE_LEN];
    printf("TinyBASIC - commands: LOAD SAVE LIST RUN NEW QUIT\n");
    if (argc == 2) {
        if (!load_file(argv[1])) { fprintf(stderr,"Failed to load %s\n", argv[1]); return 1; }
        run_program();
        return 0;
    }
    while (1) {
        printf("BASIC> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        strip_nl(line);
        char *p = trim(line);
        if (*p == '\0') continue;
        /* program line if starts with digit */
        if (isdigit((unsigned char)*p)) {
            char *sp = strchr(p, ' ');
            if (sp) {
                *sp = '\0';
                int ln = atoi(p);
                char *rest = trim(sp + 1);
                if (*rest == '\0') delete_line(ln);
                else insert_or_replace_line(ln, rest);
            } else {
                int ln = atoi(p);
                delete_line(ln);
            }
            continue;
        }
        /* commands */
        char cmd[16];
        sscanf(p, "%15s", cmd);
        for (char *q = cmd; *q; ++q) *q = toupper((unsigned char)*q);
        if (strcmp(cmd, "LOAD") == 0) {
            char fname[256];
            if (sscanf(p + 4, "%255s", fname) == 1) {
                if (!load_file(fname)) fprintf(stderr,"Failed to load %s\n", fname);
            } else fprintf(stderr,"Usage: LOAD filename\n");
        } else if (strcmp(cmd, "SAVE") == 0) {
            char fname[256];
            if (sscanf(p + 4, "%255s", fname) == 1) {
                if (!save_file(fname)) fprintf(stderr,"Failed to save %s\n", fname);
            } else fprintf(stderr,"Usage: SAVE filename\n");
        } else if (strcmp(cmd, "LIST") == 0) {
            list_program(stdout);
        } else if (strcmp(cmd, "RUN") == 0) {
            run_program();
        } else if (strcmp(cmd, "NEW") == 0) {
            do_new();
        } else if (strcmp(cmd, "QUIT") == 0 || strcmp(cmd, "EXIT") == 0) {
            break;
        } else {
            fprintf(stderr,"Unknown command: %s\n", cmd);
        }
    }

    /* cleanup */
    do_new();
    return 0;
}
// ...existing code...