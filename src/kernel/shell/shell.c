#include "shell.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "exec/executor.h"
#include "parser/lexer.h"
#include "parser/parser.h"

int gs_shell_init(struct gs_shell *shell, const char *progname) {
    if (!shell) {
        return GS_ERR_ALLOC;
    }
    shell->progname = progname;
    shell->last_status = 0;
    shell->exit_requested = false;
    shell->exit_status = 0;
    shell->interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

    struct sigaction sa = {0};
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGQUIT, &sa, NULL);

    sa.sa_handler = SIG_DFL;
    sigaction(SIGINT, &sa, NULL);
    return GS_OK;
}

void gs_shell_destroy(struct gs_shell *shell) {
    (void)shell;
}

static int run_line(struct gs_shell *shell, const char *line) {
    gs_token_buffer tokens = {0};
    int rc = gs_lexer_tokenize(line, &tokens);
    if (rc != GS_OK) {
        fprintf(stderr, "genshell: failed to lex input\n");
        gs_token_buffer_dispose(&tokens);
        shell->last_status = 1;
        return rc;
    }

    if (tokens.length == 1u && tokens.items[0].type == GS_TOKEN_END) {
        gs_token_buffer_dispose(&tokens);
        return GS_OK;
    }

    gs_pipeline pipeline = {0};
    rc = gs_parse_tokens(&tokens, &pipeline);
    gs_token_buffer_dispose(&tokens);

    if (rc != GS_OK) {
        fprintf(stderr, "genshell: syntax error\n");
        shell->last_status = 2;
        return rc;
    }

    rc = gs_execute_pipeline(shell, &pipeline);
    gs_pipeline_dispose(&pipeline);
    return rc;
}

int gs_shell_run(struct gs_shell *shell) {
    if (!shell) {
        return GS_ERR_ALLOC;
    }

    char *line = NULL;
    size_t cap = 0u;

    while (!shell->exit_requested) {
        if (shell->interactive) {
            fputs("genshell$ ", stdout);
            fflush(stdout);
        }

        errno = 0;
        ssize_t n = getline(&line, &cap, stdin);
        if (n < 0) {
            if (feof(stdin)) {
                if (shell->interactive) {
                    fputc('\n', stdout);
                }
                break;
            }
            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            perror("genshell: getline");
            shell->last_status = 1;
            break;
        }

        if (n == 0) {
            continue;
        }

        int line_status = run_line(shell, line);
        if (line_status < 0) {
            shell->last_status = 1;
        }
    }

    free(line);
    return shell->exit_requested ? shell->exit_status : shell->last_status;
}
