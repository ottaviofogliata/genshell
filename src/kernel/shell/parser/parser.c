#include "parser.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const gs_token_buffer *tokens;
    size_t index;
} parse_state;

static const gs_token *peek(const parse_state *st) {
    if (st->index >= st->tokens->length) {
        return NULL;
    }
    return &st->tokens->items[st->index];
}

static const gs_token *consume(parse_state *st) {
    const gs_token *token = peek(st);
    if (!token) {
        return NULL;
    }
    st->index++;
    return token;
}

typedef struct {
    gs_simple_command *items;
    size_t length;
    size_t capacity;
} command_vec;

typedef struct {
    gs_redirection *items;
    size_t length;
    size_t capacity;
} redir_vec;

typedef struct {
    char **items;
    size_t length;
    size_t capacity;
} string_vec;

static void string_vec_dispose(string_vec *vec) {
    if (!vec) {
        return;
    }
    for (size_t i = 0; i < vec->length; ++i) {
        free(vec->items[i]);
    }
    free(vec->items);
    vec->items = NULL;
    vec->length = 0u;
    vec->capacity = 0u;
}

static int string_vec_push(string_vec *vec, char *value) {
    if (vec->length + 1u > vec->capacity) {
        size_t new_cap = vec->capacity ? vec->capacity * 2u : 4u;
        char **tmp = (char **)realloc(vec->items, new_cap * sizeof(char *));
        if (!tmp) {
            return GS_ERR_ALLOC;
        }
        vec->items = tmp;
        vec->capacity = new_cap;
    }
    vec->items[vec->length++] = value;
    return GS_OK;
}

static void redir_vec_dispose(redir_vec *vec) {
    if (!vec) {
        return;
    }
    for (size_t i = 0; i < vec->length; ++i) {
        free(vec->items[i].target);
    }
    free(vec->items);
    vec->items = NULL;
    vec->length = 0u;
    vec->capacity = 0u;
}

static int redir_vec_push(redir_vec *vec, const gs_redirection *redir) {
    if (vec->length + 1u > vec->capacity) {
        size_t new_cap = vec->capacity ? vec->capacity * 2u : 2u;
        gs_redirection *tmp = (gs_redirection *)realloc(vec->items, new_cap * sizeof(gs_redirection));
        if (!tmp) {
            return GS_ERR_ALLOC;
        }
        vec->items = tmp;
        vec->capacity = new_cap;
    }
    vec->items[vec->length++] = *redir;
    return GS_OK;
}

static void command_vec_dispose(command_vec *vec) {
    if (!vec) {
        return;
    }
    for (size_t i = 0; i < vec->length; ++i) {
        gs_simple_command *cmd = &vec->items[i];
        for (size_t j = 0; j < cmd->argc; ++j) {
            free(cmd->argv[j]);
        }
        free(cmd->argv);
        for (size_t j = 0; j < cmd->redir_count; ++j) {
            free(cmd->redirs[j].target);
        }
        free(cmd->redirs);
    }
    free(vec->items);
    vec->items = NULL;
    vec->length = 0u;
    vec->capacity = 0u;
}

static int command_vec_push(command_vec *vec, const gs_simple_command *cmd) {
    if (vec->length + 1u > vec->capacity) {
        size_t new_cap = vec->capacity ? vec->capacity * 2u : 2u;
        gs_simple_command *tmp = (gs_simple_command *)realloc(vec->items, new_cap * sizeof(gs_simple_command));
        if (!tmp) {
            return GS_ERR_ALLOC;
        }
        vec->items = tmp;
        vec->capacity = new_cap;
    }
    vec->items[vec->length++] = *cmd;
    return GS_OK;
}

static int parse_io_number(const char *text, int *out_fd) {
    errno = 0;
    char *end = NULL;
    long val = strtol(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' || val < 0 || val > 9) {
        return GS_ERR_PARSE;
    }
    *out_fd = (int)val;
    return GS_OK;
}

static int parse_redirection(parse_state *st, gs_redirection_type type, int default_fd, int explicit_fd, gs_redirection *out_redir) {
    const gs_token *target_token = consume(st);
    if (!target_token || target_token->type != GS_TOKEN_WORD || !target_token->lexeme) {
        return GS_ERR_PARSE;
    }
    char *dup = strdup(target_token->lexeme);
    if (!dup) {
        return GS_ERR_ALLOC;
    }
    out_redir->fd = explicit_fd >= 0 ? explicit_fd : default_fd;
    out_redir->type = type;
    out_redir->target = dup;
    return GS_OK;
}

static int parse_simple_command(parse_state *st, gs_simple_command *out_cmd) {
    memset(out_cmd, 0, sizeof(*out_cmd));
    string_vec argv_vec = {0};
    redir_vec redirs = {0};
    int rc = GS_OK;

    while (true) {
        const gs_token *token = peek(st);
        if (!token) {
            rc = GS_ERR_PARSE;
            break;
        }
        if (token->type == GS_TOKEN_WORD) {
            char *dup = strdup(token->lexeme ? token->lexeme : "");
            if (!dup) {
                rc = GS_ERR_ALLOC;
                break;
            }
            rc = string_vec_push(&argv_vec, dup);
            if (rc != GS_OK) {
                free(dup);
                break;
            }
            (void)consume(st);
            continue;
        }
        if (token->type == GS_TOKEN_IO_NUMBER) {
            int fd = 0;
            rc = parse_io_number(token->lexeme, &fd);
            if (rc != GS_OK) {
                break;
            }
            (void)consume(st);
            const gs_token *op = peek(st);
            if (!op) {
                rc = GS_ERR_PARSE;
                break;
            }
            gs_redirection redir = {0};
            switch (op->type) {
            case GS_TOKEN_REDIR_IN:
                (void)consume(st);
                rc = parse_redirection(st, GS_REDIR_STDIN, 0, fd, &redir);
                break;
            case GS_TOKEN_REDIR_OUT:
                (void)consume(st);
                rc = parse_redirection(st, GS_REDIR_STDOUT, 1, fd, &redir);
                break;
            case GS_TOKEN_REDIR_APPEND:
                (void)consume(st);
                rc = parse_redirection(st, GS_REDIR_STDOUT_APPEND, 1, fd, &redir);
                break;
            default:
                rc = GS_ERR_PARSE;
                break;
            }
            if (rc != GS_OK) {
                break;
            }
            rc = redir_vec_push(&redirs, &redir);
            if (rc != GS_OK) {
                free(redir.target);
                break;
            }
            continue;
        }
        if (token->type == GS_TOKEN_REDIR_IN || token->type == GS_TOKEN_REDIR_OUT || token->type == GS_TOKEN_REDIR_APPEND) {
            gs_redirection redir = {0};
            gs_redirection_type rtype = GS_REDIR_STDOUT;
            int default_fd = 1;
            if (token->type == GS_TOKEN_REDIR_IN) {
                rtype = GS_REDIR_STDIN;
                default_fd = 0;
            } else if (token->type == GS_TOKEN_REDIR_APPEND) {
                rtype = GS_REDIR_STDOUT_APPEND;
                default_fd = 1;
            }
            (void)consume(st);
            rc = parse_redirection(st, rtype, default_fd, -1, &redir);
            if (rc != GS_OK) {
                break;
            }
            rc = redir_vec_push(&redirs, &redir);
            if (rc != GS_OK) {
                free(redir.target);
                break;
            }
            continue;
        }
        break;
    }

    if (rc != GS_OK) {
        string_vec_dispose(&argv_vec);
        redir_vec_dispose(&redirs);
        return rc;
    }

    if (argv_vec.length == 0u && redirs.length == 0u) {
        string_vec_dispose(&argv_vec);
        redir_vec_dispose(&redirs);
        return GS_ERR_PARSE;
    }

    rc = string_vec_push(&argv_vec, NULL);
    if (rc != GS_OK) {
        string_vec_dispose(&argv_vec);
        redir_vec_dispose(&redirs);
        return rc;
    }

    out_cmd->argc = argv_vec.length - 1u;
    out_cmd->argv = argv_vec.items;
    out_cmd->redirs = redirs.items;
    out_cmd->redir_count = redirs.length;
    return GS_OK;
}

int gs_parse_tokens(const gs_token_buffer *tokens, gs_pipeline *out_pipeline) {
    memset(out_pipeline, 0, sizeof(*out_pipeline));
    parse_state st = {tokens, 0};
    command_vec commands = {0};
    int rc;

    while (true) {
        gs_simple_command cmd;
        rc = parse_simple_command(&st, &cmd);
        if (rc != GS_OK) {
            command_vec_dispose(&commands);
            return rc;
        }
        rc = command_vec_push(&commands, &cmd);
        if (rc != GS_OK) {
            for (size_t i = 0; i < cmd.argc; ++i) {
                free(cmd.argv[i]);
            }
            free(cmd.argv);
            for (size_t i = 0; i < cmd.redir_count; ++i) {
                free(cmd.redirs[i].target);
            }
            free(cmd.redirs);
            command_vec_dispose(&commands);
            return rc;
        }

        const gs_token *next = peek(&st);
        if (!next) {
            command_vec_dispose(&commands);
            return GS_ERR_PARSE;
        }
        if (next->type == GS_TOKEN_PIPE) {
            (void)consume(&st);
            continue;
        }
        if (next->type == GS_TOKEN_BACKGROUND) {
            out_pipeline->background = true;
            (void)consume(&st);
        }
        if (next->type == GS_TOKEN_SEMICOLON) {
            out_pipeline->terminator = true;
            (void)consume(&st);
        }
        break;
    }

    const gs_token *terminal = peek(&st);
    if (!terminal || terminal->type != GS_TOKEN_END) {
        command_vec_dispose(&commands);
        return GS_ERR_PARSE;
    }

    out_pipeline->commands = commands.items;
    out_pipeline->length = commands.length;
    return GS_OK;
}

void gs_pipeline_dispose(gs_pipeline *pipeline) {
    if (!pipeline || !pipeline->commands) {
        return;
    }
    for (size_t i = 0; i < pipeline->length; ++i) {
        gs_simple_command *cmd = &pipeline->commands[i];
        if (cmd->argv) {
            for (size_t j = 0; j < cmd->argc; ++j) {
                free(cmd->argv[j]);
            }
            free(cmd->argv);
        }
        if (cmd->redirs) {
            for (size_t j = 0; j < cmd->redir_count; ++j) {
                free(cmd->redirs[j].target);
            }
            free(cmd->redirs);
        }
    }
    free(pipeline->commands);
    pipeline->commands = NULL;
    pipeline->length = 0u;
    pipeline->background = false;
    pipeline->terminator = false;
}
