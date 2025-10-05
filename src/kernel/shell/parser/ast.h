#ifndef GS_PARSER_AST_H
#define GS_PARSER_AST_H

#include <stdbool.h>
#include <stddef.h>

#include "../shell.h"

typedef enum {
    GS_REDIR_STDIN,
    GS_REDIR_STDOUT,
    GS_REDIR_STDOUT_APPEND,
    GS_REDIR_STDERR
} gs_redirection_type;

typedef struct {
    int fd;
    gs_redirection_type type;
    char *target; /* owned */
} gs_redirection;

typedef struct {
    char **argv;        /* NULL-terminated, owned */
    size_t argc;
    gs_redirection *redirs;
    size_t redir_count;
} gs_simple_command;

typedef struct {
    gs_simple_command *commands;
    size_t length;
    bool background;
    bool terminator; /* true if command ended with ';' */
} gs_pipeline;

void gs_pipeline_dispose(gs_pipeline *pipeline);

#endif /* GS_PARSER_AST_H */
