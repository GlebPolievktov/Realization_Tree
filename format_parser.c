#include "format_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static int parse_whitespace(const char **p, token_t *token);
static int parse_literal(const char **p, token_t *token);
static int parse_conversion(const char **p, token_t *token);
static int parse_scanset(const char **p, token_t *token);
static length_t parse_length_modifier(const char **p);
static int ensure_capacity(token_t **tokens, size_t *capacity, size_t count);

static int ensure_capacity(token_t **tokens, size_t *capacity, size_t count) {
    if (count >= *capacity) {
        *capacity *= 2;
        token_t *new_tokens = realloc(*tokens, *capacity * sizeof(token_t));
        if (!new_tokens) {
            free(*tokens);
            return -1;
        }
        *tokens = new_tokens;
    }
    return 0;
}

static int parse_whitespace(const char **p, token_t *token) {
    while (isspace(**p)) {
        (*p)++;
    }
    *token = (token_t){
        .type = TOK_WS,
        .lit = NULL,
        .lit_len = 0,
        .is_suppress = false,
        .width = 0,
        .length = LEN_NONE,
        .spec = 0,
        .scanset_table = {0},
        .is_scanset_invert = false
    };
    return 0;
}

static int parse_literal(const char **p, token_t *token) {
    const char *start = *p;
    while (**p && !isspace(**p) && **p != '%') {
        (*p)++;
    }
    size_t lit_len = *p - start;
    
    *token = (token_t){
        .type = TOK_LITERAL,
        .lit = start,
        .lit_len = lit_len,
        .is_suppress = false,
        .width = 0,
        .length = LEN_NONE,
        .spec = 0,
        .scanset_table = {0},
        .is_scanset_invert = false
    };
    return 0;
}

static length_t parse_length_modifier(const char **p) {
    length_t result = LEN_NONE;
    
    if (strncmp(*p, "hh", 2) == 0) {
        *p += 2;
        result = LEN_HH;
    } else if (strncmp(*p, "ll", 2) == 0) {
        *p += 2;
        result = LEN_LL;
    } else if (**p == 'h') {
        (*p)++;
        result = LEN_H;
    } else if (**p == 'l') {
        (*p)++;
        result = LEN_L;
    } else if (**p == 'L') {
        (*p)++;
        result = LEN_CAP_L;
    }
    
    return result;
}

static int parse_scanset(const char **p, token_t *token) {
    bool is_invert = false;
    int result = 0;
    
    if (**p == '^') {
        is_invert = true;
        (*p)++;
    }
    
    unsigned char temp_scanset_table[256];
    memset(temp_scanset_table, 0, sizeof(temp_scanset_table));
    
    bool first_char = true;
    while (**p && **p != ']') {
        if (**p == '-' && !first_char && *(*p + 1) && *(*p + 1) != ']') {
            unsigned char start = *(*p - 1);
            unsigned char end = *(*p + 1);
            *p += 2; 
            
            if (start <= end) {
                for (unsigned char c = start; c <= end; c++) {
                    temp_scanset_table[c] = 1;
                }
            }
        } else {
            temp_scanset_table[(unsigned char)**p] = 1;
            (*p)++;
        }
        first_char = false;
    }

    if (**p != ']') {
        result = -5;
    } else {
        (*p)++;
        
        *token = (token_t){
            .type = TOK_CONV,
            .lit = NULL,
            .lit_len = 0,
            .is_suppress = false, 
            .width = 0,           
            .length = LEN_NONE,   
            .spec = '[',
            .scanset_table = {0},
            .is_scanset_invert = is_invert
        };
        
        memcpy(token->scanset_table, temp_scanset_table, 256);
        result = 0;
    }
    
    return result;
}

static int parse_conversion(const char **p, token_t *token) {
    int result = 0;
    
    if (**p == '\0') {
        result = -2;
    } else if (**p == '%') {
        *token = (token_t){
            .type = TOK_LITERAL,
            .lit = *p - 1, 
            .lit_len = 1,
            .is_suppress = false,
            .width = 0,
            .length = LEN_NONE,
            .spec = 0,
            .scanset_table = {0},
            .is_scanset_invert = false
        };
        (*p)++;
        result = 0;
    } else {
        bool is_suppress = false;
        size_t width = 0;
        length_t length = LEN_NONE;

        if (**p == '*') {
            is_suppress = true;
            (*p)++;
        }

        while (isdigit(**p) && result == 0) {
            if (width > SIZE_MAX / 10) {
                result = -3;
            } else {
                width = width * 10 + (**p - '0');
                (*p)++;
            }
        }

        if (result == 0) {
            length = parse_length_modifier(p);

            if (**p == '\0') {
                result = -4;
            } else {
                char spec = **p;
                (*p)++;

                if (spec == '[') {
                    result = parse_scanset(p, token);
                    if (result == 0) {
                        token->is_suppress = is_suppress;
                        token->width = width;
                        token->length = length;
                    }
                } else {
                    *token = (token_t){
                        .type = TOK_CONV,
                        .lit = NULL,
                        .lit_len = 0,
                        .is_suppress = is_suppress,
                        .width = width,
                        .length = length,
                        .spec = spec,
                        .scanset_table = {0},
                        .is_scanset_invert = false
                    };
                    result = 0;
                }
            }
        }
    }
    
    return result;
}

int parse_format(const char *fmt, token_t **tokens_out, size_t *ntokens_out) {
    int result = 0;
    
    if (!fmt || !tokens_out || !ntokens_out) {
        result = -1;
    } else {
        size_t count = 0;
        const char *p = fmt;
        size_t capacity = 10;
        
        token_t *tokens = malloc(capacity * sizeof(token_t));
        if (!tokens) {
            result = -1;
        } else {
            while (*p && result == 0) {
                if (ensure_capacity(&tokens, &capacity, count) != 0) {
                    free(tokens);
                    result = -1;
                } else {
                    token_t token;

                    if (isspace(*p)) {
                        result = parse_whitespace(&p, &token);
                    } else if (*p == '%') {
                        p++; 
                        result = parse_conversion(&p, &token);
                    } else {
                        result = parse_literal(&p, &token);
                    }

                    if (result != 0) {
                        free(tokens);
                    } else {
                        tokens[count++] = token;
                    }
                }
            }

            if (result == 0) {
                *tokens_out = tokens;
                *ntokens_out = count;
            }
        }
    }
    
    return result;
}

void free_format_tokens(token_t *tokens, size_t ntokens) {
    (void)ntokens; 
    if (tokens) {
        free(tokens);
    }
}
