#ifndef GS_PARSER_PARSER_H
#define GS_PARSER_PARSER_H

#include "ast.h"
#include "lexer.h"

int gs_parse_tokens(const gs_token_buffer *tokens, gs_pipeline *out_pipeline);

#endif /* GS_PARSER_PARSER_H */
