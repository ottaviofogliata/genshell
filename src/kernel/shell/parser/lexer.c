#include "lexer.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int ensure_capacity(gs_token_buffer *buf, size_t needed) {
    if (buf->capacity >= needed) {
        return GS_OK;
    }
    size_t new_cap = buf->capacity ? buf->capacity : 8u;
    while (new_cap < needed) {
        new_cap *= 2u;
    }
    gs_token *items = (gs_token *)realloc(buf->items, new_cap * sizeof(gs_token));
    if (!items) {
        return GS_ERR_ALLOC;
    }
    buf->items = items;
    buf->capacity = new_cap;
    return GS_OK;
}

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    unsigned flags;
} word_builder;

static void builder_dispose(word_builder *builder) {
    free(builder->data);
    builder->data = NULL;
    builder->length = 0u;
    builder->capacity = 0u;
    builder->flags = 0u;
}

static int builder_reserve(word_builder *builder, size_t needed) {
    if (builder->capacity >= needed) {
        return GS_OK;
    }
    size_t new_cap = builder->capacity ? builder->capacity : 32u;
    while (new_cap < needed) {
        new_cap *= 2u;
    }
    char *tmp = (char *)realloc(builder->data, new_cap);
    if (!tmp) {
        return GS_ERR_ALLOC;
    }
    builder->data = tmp;
    builder->capacity = new_cap;
    return GS_OK;
}

static int builder_append_raw(word_builder *builder, char ch) {
    int rc = builder_reserve(builder, builder->length + 2u);
    if (rc != GS_OK) {
        return rc;
    }
    builder->data[builder->length++] = ch;
    builder->data[builder->length] = '\0';
    return GS_OK;
}

static int builder_append_literal(word_builder *builder, char ch) {
    int rc = builder_reserve(builder, builder->length + 3u);
    if (rc != GS_OK) {
        return rc;
    }
    builder->data[builder->length++] = GS_LITERAL_SENTINEL;
    builder->data[builder->length++] = ch;
    builder->data[builder->length] = '\0';
    builder->flags |= GS_TOKEN_FLAG_HAS_SENTINEL;
    return GS_OK;
}

static int builder_finalize(word_builder *builder, gs_token *out_token) {
    int rc = builder_reserve(builder, builder->length + 1u);
    if (rc != GS_OK) {
        return rc;
    }
    builder->data[builder->length] = '\0';
    out_token->type = GS_TOKEN_WORD;
    out_token->lexeme = builder->data;
    out_token->flags = builder->flags;
    builder->data = NULL;
    builder->length = 0u;
    builder->capacity = 0u;
    builder->flags = 0u;
    return GS_OK;
}

static int buffer_append(gs_token_buffer *buf, const gs_token *token) {
    int rc = ensure_capacity(buf, buf->length + 1u);
    if (rc != GS_OK) {
        return rc;
    }
    buf->items[buf->length++] = *token;
    return GS_OK;
}

static int append_simple_token(gs_token_buffer *buf, gs_token_type type) {
    gs_token token;
    token.type = type;
    token.lexeme = NULL;
    token.flags = 0u;
    return buffer_append(buf, &token);
}

static int emit_word_token(gs_token_buffer *buf, word_builder *builder) {
    gs_token token;
    int rc = builder_finalize(builder, &token);
    if (rc != GS_OK) {
        return rc;
    }
    rc = buffer_append(buf, &token);
    if (rc != GS_OK) {
        free(token.lexeme);
    }
    return rc;
}

static int lex_word(const char **cursor, gs_token_buffer *buf) {
    const char *p = *cursor;
    int rc;
    word_builder builder = {0};
    bool in_single = false;
    bool in_double = false;

    while (*p) {
        char ch = *p;
        if (!in_single && !in_double) {
            if (isspace((unsigned char)ch) || ch == '|' || ch == '&' || ch == ';' || ch == '<' || ch == '>') {
                break;
            }
        }
        if (!in_single && ch == '\'') {
            in_single = true;
            ++p;
            continue;
        }
        if (!in_single && ch == '"') {
            in_double = !in_double;
            ++p;
            continue;
        }
        if (in_single && ch == '\'') {
            in_single = false;
            ++p;
            continue;
        }
        if (!in_single && ch == '\\') {
            ++p;
            char next = *p;
            if (next == '\0') {
                builder_dispose(&builder);
                return GS_ERR_PARSE;
            }
            if (next == '\n') {
                ++p;
                continue; /* line continuation */
            }
            rc = builder_append_literal(&builder, next);
            if (rc != GS_OK) {
                builder_dispose(&builder);
                return rc;
            }
            ++p;
            continue;
        }
        if (ch == '\n' && !in_single && !in_double) {
            break;
        }
        if (!in_single && !in_double && ch == '#') {
            /* treat as literal when appearing mid-word */
            rc = builder_append_literal(&builder, ch);
            if (rc != GS_OK) {
                builder_dispose(&builder);
                return rc;
            }
            ++p;
            continue;
        }
        if (in_single) {
            rc = builder_append_literal(&builder, ch);
        } else {
            rc = builder_append_raw(&builder, ch);
        }
        if (rc != GS_OK) {
            builder_dispose(&builder);
            return rc;
        }
        ++p;
    }

    if (in_single || in_double) {
        builder_dispose(&builder);
        return GS_ERR_PARSE;
    }

    rc = emit_word_token(buf, &builder);
    if (rc != GS_OK) {
        builder_dispose(&builder);
        return rc;
    }
    *cursor = p;
    return GS_OK;
}

int gs_lexer_tokenize(const char *line, gs_token_buffer *out_tokens) {
    memset(out_tokens, 0, sizeof(*out_tokens));
    if (!line) {
        return GS_ERR_PARSE;
    }

    const char *p = line;
    bool at_boundary = true;

    while (*p) {
        char ch = *p;
        if (isspace((unsigned char)ch)) {
            ++p;
            if (ch == '\n') {
                break;
            }
            at_boundary = true;
            continue;
        }
        if (ch == '#') {
            if (at_boundary) {
                break; /* comment to end of line */
            }
            /* otherwise treat as literal within word */
        }

        if (isdigit((unsigned char)ch)) {
            const char *look = p;
            while (isdigit((unsigned char)*look)) {
                ++look;
            }
            if (*look == '>' || *look == '<') {
                size_t len = (size_t)(look - p);
                char *digits = (char *)malloc(len + 1u);
                if (!digits) {
                    gs_token_buffer_dispose(out_tokens);
                    return GS_ERR_ALLOC;
                }
                memcpy(digits, p, len);
                digits[len] = '\0';
                gs_token token;
                token.type = GS_TOKEN_IO_NUMBER;
                token.lexeme = digits;
                token.flags = 0u;
                int rc = buffer_append(out_tokens, &token);
                if (rc != GS_OK) {
                    free(digits);
                    gs_token_buffer_dispose(out_tokens);
                    return rc;
                }
                p = look;
                at_boundary = false;
                continue;
            }
        }

        switch (ch) {
        case '|': {
            int rc = append_simple_token(out_tokens, GS_TOKEN_PIPE);
            if (rc != GS_OK) {
                gs_token_buffer_dispose(out_tokens);
                return rc;
            }
            ++p;
            at_boundary = true;
            continue;
        }
        case '&': {
            int rc = append_simple_token(out_tokens, GS_TOKEN_BACKGROUND);
            if (rc != GS_OK) {
                gs_token_buffer_dispose(out_tokens);
                return rc;
            }
            ++p;
            at_boundary = true;
            continue;
        }
        case ';': {
            int rc = append_simple_token(out_tokens, GS_TOKEN_SEMICOLON);
            if (rc != GS_OK) {
                gs_token_buffer_dispose(out_tokens);
                return rc;
            }
            ++p;
            at_boundary = true;
            continue;
        }
        case '<': {
            int rc = append_simple_token(out_tokens, GS_TOKEN_REDIR_IN);
            if (rc != GS_OK) {
                gs_token_buffer_dispose(out_tokens);
                return rc;
            }
            ++p;
            at_boundary = true;
            continue;
        }
        case '>': {
            ++p;
            gs_token_type type = GS_TOKEN_REDIR_OUT;
            if (*p == '>') {
                type = GS_TOKEN_REDIR_APPEND;
                ++p;
            }
            int rc = append_simple_token(out_tokens, type);
            if (rc != GS_OK) {
                gs_token_buffer_dispose(out_tokens);
                return rc;
            }
            at_boundary = true;
            continue;
        }
        default:
            break;
        }

        int rc = lex_word(&p, out_tokens);
        if (rc != GS_OK) {
            gs_token_buffer_dispose(out_tokens);
            return rc;
        }
        at_boundary = false;
    }

    int rc = append_simple_token(out_tokens, GS_TOKEN_END);
    if (rc != GS_OK) {
        gs_token_buffer_dispose(out_tokens);
        return rc;
    }
    return GS_OK;
}

void gs_token_buffer_dispose(gs_token_buffer *buf) {
    if (!buf || !buf->items) {
        return;
    }
    for (size_t i = 0; i < buf->length; ++i) {
        free(buf->items[i].lexeme);
        buf->items[i].lexeme = NULL;
    }
    free(buf->items);
    buf->items = NULL;
    buf->length = 0u;
    buf->capacity = 0u;
}
