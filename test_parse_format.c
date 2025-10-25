#include "format_parser.h"
#include <stdio.h>
#include <stdlib.h>

void print_token(const token_t *token, size_t index) {
    printf("Token %zu: ", index);
    
    switch (token->type) {
        case TOK_LITERAL:
            printf("LITERAL '%.*s' (len=%zu)", (int)token->lit_len, token->lit, token->lit_len);
            break;
        case TOK_WS:
            printf("WHITESPACE");
            break;
        case TOK_CONV:
            printf("CONVERSION spec='%c'", token->spec);
            if (token->is_suppress) printf(" suppress");
            if (token->width > 0) printf(" width=%zu", token->width);
            if (token->length != LEN_NONE) {
                const char *len_str[] = {"", "hh", "h", "l", "ll", "L"};
                printf(" length=%s", len_str[token->length]);
            }
            if (token->spec == '[') {
                printf(" scanset_invert=%s", token->is_scanset_invert ? "true" : "false");
                printf(" scanset_chars=");
                for (int i = 0; i < 256; i++) {
                    if (token->scanset_table[i]) {
                        printf("%c", (char)i);
                    }
                }
            }
            break;
    }
    printf("\n");
}

void test_format(const char *format, const char *description) {
    printf("\n=== %s ===\n", description);
    printf("Format: \"%s\"\n", format);
    
    token_t *tokens;
    size_t ntokens;
    
    int result = parse_format(format, &tokens, &ntokens);
    
    if (result == 0) {
        printf("✓ Success: %zu tokens\n", ntokens);
        for (size_t i = 0; i < ntokens; i++) {
            print_token(&tokens[i], i);
        }
        free_format_tokens(tokens, ntokens);
    } else {
        printf("✗ Error parsing format: %d\n", result);
    }
}

int main() {
    printf("=== Testing parse_format function ===\n");
    
    // Базовые тесты
    test_format("%d", "Simple integer conversion");
    test_format("%s", "Simple string conversion");
    test_format("%c", "Simple character conversion");
    
    // Тесты с width
    test_format("%5d", "Integer with width");
    test_format("%10s", "String with width");
    test_format("%3c", "Character with width");
    
    // Тесты с suppress
    test_format("%*d", "Suppressed integer");
    test_format("%*s", "Suppressed string");
    
    // Тесты с модификаторами длины
    test_format("%hd", "Short integer");
    test_format("%ld", "Long integer");
    test_format("%lld", "Long long integer");
    test_format("%Lf", "Long double");
    test_format("%hhx", "Unsigned char hex");
    
    // Тесты с пробелами
    test_format("%d %s", "Integer and string with space");
    test_format("  %d  %s  ", "Multiple spaces");
    test_format("\t%d\n%s", "Tab and newline");
    
    // Тесты с литералами
    test_format("Name:%s", "Literal with conversion");
    test_format("Age:%d Name:%s", "Multiple literals");
    test_format("Value: %d", "Literal with space");
    
    // Тесты с %% (literal %)
    test_format("%%", "Literal percent");
    test_format("%d%%", "Integer with literal percent");
    test_format("%%d", "Literal percent with conversion");
    
    // Тесты с scanset
    test_format("%[abc]", "Simple scanset");
    test_format("%[a-z]", "Scanset with range");
    test_format("%[a-zA-Z0-9]", "Scanset with multiple ranges");
    test_format("%[^abc]", "Inverted scanset");
    test_format("%[^]]", "Inverted scanset with ]");
    test_format("%[a-z-]", "Scanset with dash at end");
    test_format("%[-a-z]", "Scanset with dash at start");
    
    // Комплексные тесты
    test_format("%d %5s %*c", "Complex format 1");
    test_format("Name:%s Age:%d", "Complex format 2");
    test_format("%[a-zA-Z0-9_] %*d %%", "Complex format 3");
    test_format("%3c %llx %*s", "Complex format 4");
    
    // Тесты на ошибки
    printf("\n=== Error Tests ===\n");
    test_format("%", "Incomplete format (should fail)");
    test_format("%[abc", "Unclosed scanset (should fail)");
    
    printf("\n=== All tests completed ===\n");
    return 0;
}



