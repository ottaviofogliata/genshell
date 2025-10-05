#ifndef GS_PARSER_LEXER_H
#define GS_PARSER_LEXER_H

#include <stddef.h>

#include "../shell.h"

typedef enum {
    GS_TOKEN_WORD,
    GS_TOKEN_PIPE,
    GS_TOKEN_BACKGROUND,
    GS_TOKEN_SEMICOLON,
    GS_TOKEN_REDIR_IN,
    GS_TOKEN_REDIR_OUT,
    GS_TOKEN_REDIR_APPEND,
    GS_TOKEN_REDIR_ERR,
    GS_TOKEN_IO_NUMBER,
    GS_TOKEN_END
} gs_token_type;

#define GS_TOKEN_FLAG_HAS_SENTINEL 0x01u

typedef struct {
    gs_token_type type;
    char *lexeme;
    unsigned flags;
} gs_token;

typedef struct {
    gs_token *items;
    size_t length;
    size_t capacity;
} gs_token_buffer;

int gs_lexer_tokenize(const char *line, gs_token_buffer *out_tokens);
void gs_token_buffer_dispose(gs_token_buffer *buf);

#endif /* GS_PARSER_LEXER_H */
