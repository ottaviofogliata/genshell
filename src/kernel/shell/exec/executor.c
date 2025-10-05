#include "executor.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../builtins/builtin.h"

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} gs_strbuf;

typedef struct {
    gs_redirection_type type;
    int fd;
    char *target;
} gs_expanded_redir;

typedef struct {
    char **argv;
    size_t argc;
    gs_expanded_redir *redirs;
    size_t redir_count;
    const gs_builtin_spec *builtin;
} gs_prepared_command;

typedef struct {
    int fd;
    int saved_fd;
} saved_descriptor;

static void gs_strbuf_dispose(gs_strbuf *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->length = 0u;
    buf->capacity = 0u;
}

static int gs_strbuf_reserve(gs_strbuf *buf, size_t extra) {
    size_t needed = buf->length + extra + 1u;
    if (buf->capacity >= needed) {
        return GS_OK;
    }
    size_t new_cap = buf->capacity ? buf->capacity : 32u;
    while (new_cap < needed) {
        new_cap *= 2u;
    }
    char *tmp = (char *)realloc(buf->data, new_cap);
    if (!tmp) {
        return GS_ERR_ALLOC;
    }
    buf->data = tmp;
    buf->capacity = new_cap;
    return GS_OK;
}

static int gs_strbuf_append_mem(gs_strbuf *buf, const char *data, size_t len) {
    if (len == 0u) {
        return GS_OK;
    }
    int rc = gs_strbuf_reserve(buf, len);
    if (rc != GS_OK) {
        return rc;
    }
    memcpy(buf->data + buf->length, data, len);
    buf->length += len;
    buf->data[buf->length] = '\0';
    return GS_OK;
}

static int gs_strbuf_append_char(gs_strbuf *buf, char ch) {
    int rc = gs_strbuf_reserve(buf, 1u);
    if (rc != GS_OK) {
        return rc;
    }
    buf->data[buf->length++] = ch;
    buf->data[buf->length] = '\0';
    return GS_OK;
}

static int gs_strbuf_append_int(gs_strbuf *buf, long value) {
    char tmp[32];
    int len = snprintf(tmp, sizeof(tmp), "%ld", value);
    if (len < 0) {
        return GS_ERR_EXEC;
    }
    return gs_strbuf_append_mem(buf, tmp, (size_t)len);
}

static int append_parameter(gs_strbuf *buf, const struct gs_shell *shell, const char *name, size_t len) {
    if (len == 0u) {
        return gs_strbuf_append_char(buf, '$');
    }
    if (len == 1u) {
        char tag = name[0];
        switch (tag) {
        case '?':
            return gs_strbuf_append_int(buf, shell ? shell->last_status : 0);
        case '$':
            return gs_strbuf_append_int(buf, (long)getpid());
        case '0': {
            const char *prog = (shell && shell->progname) ? shell->progname : "genshell";
            return gs_strbuf_append_mem(buf, prog, strlen(prog));
        }
        default:
            if (tag >= '1' && tag <= '9') {
                return GS_OK; /* positional params not yet implemented */
            }
            break;
        }
    }
    char *key = (char *)malloc(len + 1u);
    if (!key) {
        return GS_ERR_ALLOC;
    }
    memcpy(key, name, len);
    key[len] = '\0';
    const char *value = getenv(key);
    if (!value) {
        value = "";
    }
    int rc = gs_strbuf_append_mem(buf, value, strlen(value));
    free(key);
    return rc;
}

static int expand_word(const struct gs_shell *shell, const char *word, char **out_word) {
    gs_strbuf buf = {0};
    const char *p = word;
    bool literal_next = false;

    while (p && *p) {
        char ch = *p++;
        if (literal_next) {
            int rc = gs_strbuf_append_char(&buf, ch);
            if (rc != GS_OK) {
                gs_strbuf_dispose(&buf);
                return rc;
            }
            literal_next = false;
            continue;
        }
        if (ch == GS_LITERAL_SENTINEL) {
            literal_next = true;
            continue;
        }
        if (ch == '~' && buf.length == 0u) {
            const char *home = getenv("HOME");
            if (!home) {
                home = "";
            }
            int rc = gs_strbuf_append_mem(&buf, home, strlen(home));
            if (rc != GS_OK) {
                gs_strbuf_dispose(&buf);
                return rc;
            }
            continue;
        }
        if (ch == '$') {
            if (!*p) {
                int rc = gs_strbuf_append_char(&buf, '$');
                if (rc != GS_OK) {
                    gs_strbuf_dispose(&buf);
                    return rc;
                }
                break;
            }
            if (*p == '$' || *p == '?' || *p == '0') {
                char special = *p++;
                int rc = append_parameter(&buf, shell, &special, 1u);
                if (rc != GS_OK) {
                    gs_strbuf_dispose(&buf);
                    return rc;
                }
                continue;
            }
            if (*p == '{') {
                ++p;
                const char *start = p;
                while (*p && *p != '}') {
                    ++p;
                }
                if (*p != '}') {
                    int rc = gs_strbuf_append_char(&buf, '$');
                    if (rc != GS_OK) {
                        gs_strbuf_dispose(&buf);
                        return rc;
                    }
                    p = start - 1;
                    continue;
                }
                size_t len = (size_t)(p - start);
                int rc = append_parameter(&buf, shell, start, len);
                if (rc != GS_OK) {
                    gs_strbuf_dispose(&buf);
                    return rc;
                }
                ++p; /* skip closing brace */
                continue;
            }
            if (isalnum((unsigned char)*p) || *p == '_') {
                const char *start = p;
                while (isalnum((unsigned char)*p) || *p == '_') {
                    ++p;
                }
                size_t len = (size_t)(p - start);
                int rc = append_parameter(&buf, shell, start, len);
                if (rc != GS_OK) {
                    gs_strbuf_dispose(&buf);
                    return rc;
                }
                continue;
            }
            int rc = gs_strbuf_append_char(&buf, '$');
            if (rc != GS_OK) {
                gs_strbuf_dispose(&buf);
                return rc;
            }
            continue;
        }
        int rc = gs_strbuf_append_char(&buf, ch);
        if (rc != GS_OK) {
            gs_strbuf_dispose(&buf);
            return rc;
        }
    }

    if (!buf.data) {
        buf.data = (char *)malloc(1u);
        if (!buf.data) {
            return GS_ERR_ALLOC;
        }
        buf.data[0] = '\0';
    }
    *out_word = buf.data;
    return GS_OK;
}

static void dispose_expanded_redirs(gs_expanded_redir *redirs, size_t count) {
    if (!redirs) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        free(redirs[i].target);
    }
    free(redirs);
}

static void dispose_prepared_command(gs_prepared_command *cmd) {
    if (!cmd) {
        return;
    }
    if (cmd->argv) {
        for (size_t i = 0; i < cmd->argc; ++i) {
            free(cmd->argv[i]);
        }
        free(cmd->argv);
    }
    dispose_expanded_redirs(cmd->redirs, cmd->redir_count);
    cmd->argv = NULL;
    cmd->argc = 0u;
    cmd->redirs = NULL;
    cmd->redir_count = 0u;
    cmd->builtin = NULL;
}

static int prepare_command(struct gs_shell *shell, const gs_simple_command *command, gs_prepared_command *out) {
    memset(out, 0, sizeof(*out));

    size_t argv_len = command->argc;
    out->argv = (char **)calloc(argv_len + 1u, sizeof(char *));
    if (!out->argv) {
        return GS_ERR_ALLOC;
    }

    for (size_t i = 0; i < command->argc; ++i) {
        char *expanded = NULL;
        int rc = expand_word(shell, command->argv[i], &expanded);
        if (rc != GS_OK) {
            dispose_prepared_command(out);
            return rc;
        }
        out->argv[i] = expanded;
    }
    out->argc = command->argc;
    out->argv[argv_len] = NULL;

    if (command->redir_count > 0u) {
        out->redirs = (gs_expanded_redir *)calloc(command->redir_count, sizeof(gs_expanded_redir));
        if (!out->redirs) {
            dispose_prepared_command(out);
            return GS_ERR_ALLOC;
        }
        out->redir_count = command->redir_count;
        for (size_t i = 0; i < command->redir_count; ++i) {
            char *expanded = NULL;
            int rc = expand_word(shell, command->redirs[i].target, &expanded);
            if (rc != GS_OK) {
                dispose_prepared_command(out);
                return rc;
            }
            out->redirs[i].type = command->redirs[i].type;
            out->redirs[i].fd = command->redirs[i].fd;
            out->redirs[i].target = expanded;
        }
    }

    const char *name = (out->argc > 0u) ? out->argv[0] : NULL;
    out->builtin = name ? gs_builtin_lookup(name) : NULL;
    return GS_OK;
}

static int open_redirection(const gs_expanded_redir *redir) {
    int flags = 0;
    mode_t mode = 0666;

    switch (redir->type) {
    case GS_REDIR_STDIN:
        flags = O_RDONLY;
        break;
    case GS_REDIR_STDOUT:
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        break;
    case GS_REDIR_STDOUT_APPEND:
        flags = O_WRONLY | O_CREAT | O_APPEND;
        break;
    case GS_REDIR_STDERR:
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        break;
    default:
        return -1;
    }

    int fd = open(redir->target, flags, mode);
    return fd;
}

static int apply_child_redirs(const gs_expanded_redir *redirs, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        int fd = open_redirection(&redirs[i]);
        if (fd < 0) {
            fprintf(stderr, "genshell: failed to open %s: %s\n", redirs[i].target, strerror(errno));
            return GS_ERR_EXEC;
        }
        if (dup2(fd, redirs[i].fd) < 0) {
            fprintf(stderr, "genshell: redirection failed: %s\n", strerror(errno));
            close(fd);
            return GS_ERR_EXEC;
        }
        close(fd);
    }
    return GS_OK;
}

static int ensure_saved_descriptor(saved_descriptor **array, size_t *length, size_t *capacity, int fd) {
    for (size_t i = 0; i < *length; ++i) {
        if ((*array)[i].fd == fd) {
            return (int)i;
        }
    }
    if (*length + 1u > *capacity) {
        size_t new_cap = *capacity ? *capacity * 2u : 4u;
        saved_descriptor *tmp = (saved_descriptor *)realloc(*array, new_cap * sizeof(saved_descriptor));
        if (!tmp) {
            return GS_ERR_ALLOC;
        }
        *array = tmp;
        *capacity = new_cap;
    }
    int dup_fd = dup(fd);
    if (dup_fd < 0) {
        return GS_ERR_EXEC;
    }
    (*array)[*length].fd = fd;
    (*array)[*length].saved_fd = dup_fd;
    (*length)++;
    return (int)(*length - 1u);
}

static int apply_parent_redirs(const gs_expanded_redir *redirs, size_t count, saved_descriptor **out_saved, size_t *out_length) {
    *out_saved = NULL;
    *out_length = 0u;
    if (count == 0u) {
        return GS_OK;
    }

    saved_descriptor *saved = NULL;
    size_t saved_len = 0u;
    size_t saved_cap = 0u;

    for (size_t i = 0; i < count; ++i) {
        int fd_index = ensure_saved_descriptor(&saved, &saved_len, &saved_cap, redirs[i].fd);
        if (fd_index < 0) {
            for (size_t j = 0; j < saved_len; ++j) {
                close(saved[j].saved_fd);
            }
            free(saved);
            return fd_index;
        }

        int fd = open_redirection(&redirs[i]);
        if (fd < 0) {
            fprintf(stderr, "genshell: failed to open %s: %s\n", redirs[i].target, strerror(errno));
            for (size_t j = 0; j < saved_len; ++j) {
                dup2(saved[j].saved_fd, saved[j].fd);
                close(saved[j].saved_fd);
            }
            free(saved);
            return GS_ERR_EXEC;
        }
        if (dup2(fd, redirs[i].fd) < 0) {
            fprintf(stderr, "genshell: redirection failed: %s\n", strerror(errno));
            close(fd);
            for (size_t j = 0; j < saved_len; ++j) {
                dup2(saved[j].saved_fd, saved[j].fd);
                close(saved[j].saved_fd);
            }
            free(saved);
            return GS_ERR_EXEC;
        }
        close(fd);
    }

    *out_saved = saved;
    *out_length = saved_len;
    return GS_OK;
}

static void restore_parent_redirs(saved_descriptor *saved, size_t count) {
    if (!saved) {
        return;
    }
    for (size_t i = count; i > 0; --i) {
        size_t idx = i - 1;
        dup2(saved[idx].saved_fd, saved[idx].fd);
        close(saved[idx].saved_fd);
    }
    free(saved);
}

static int execute_parent_builtin(struct gs_shell *shell, gs_prepared_command *cmd) {
    saved_descriptor *saved = NULL;
    size_t saved_len = 0u;
    int rc = apply_parent_redirs(cmd->redirs, cmd->redir_count, &saved, &saved_len);
    if (rc != GS_OK) {
        restore_parent_redirs(saved, saved_len);
        return rc;
    }

    int status = cmd->builtin->fn(shell, (int)cmd->argc, cmd->argv);
    restore_parent_redirs(saved, saved_len);
    return status;
}

static void execute_child_builtin(struct gs_shell *shell, gs_prepared_command *cmd) {
    int rc = apply_child_redirs(cmd->redirs, cmd->redir_count);
    if (rc != GS_OK) {
        _exit(1);
    }
    int status = cmd->builtin->fn(shell, (int)cmd->argc, cmd->argv);
    if (status < 0) {
        status = 1;
    }
    _exit(status & 0xFF);
}

static void execute_child_external(gs_prepared_command *cmd) {
    int rc = apply_child_redirs(cmd->redirs, cmd->redir_count);
    if (rc != GS_OK) {
        _exit(1);
    }
    execvp(cmd->argv[0], cmd->argv);
    int code = (errno == ENOENT) ? 127 : 126;
    fprintf(stderr, "genshell: %s: %s\n", cmd->argv[0], strerror(errno));
    _exit(code);
}

static int execute_pipeline_processes(struct gs_shell *shell, gs_prepared_command *cmds, size_t count) {
    pid_t *pids = (pid_t *)calloc(count, sizeof(pid_t));
    if (!pids) {
        return GS_ERR_ALLOC;
    }
    int prev_read = -1;
    int status_result = 0;

    for (size_t i = 0; i < count; ++i) {
        int pipefd[2] = {-1, -1};
        if (i + 1 < count) {
            if (pipe(pipefd) < 0) {
                fprintf(stderr, "genshell: pipe failed: %s\n", strerror(errno));
                if (prev_read >= 0) {
                    close(prev_read);
                    prev_read = -1;
                }
                status_result = GS_ERR_EXEC;
                goto cleanup;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "genshell: fork failed: %s\n", strerror(errno));
            if (pipefd[0] >= 0) {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            if (prev_read >= 0) {
                close(prev_read);
                prev_read = -1;
            }
            status_result = GS_ERR_EXEC;
            goto cleanup;
        }

        if (pid == 0) {
            if (prev_read >= 0 && dup2(prev_read, STDIN_FILENO) < 0) {
                fprintf(stderr, "genshell: dup2 failed: %s\n", strerror(errno));
                _exit(1);
            }
            if (pipefd[1] >= 0 && dup2(pipefd[1], STDOUT_FILENO) < 0) {
                fprintf(stderr, "genshell: dup2 failed: %s\n", strerror(errno));
                _exit(1);
            }

            if (pipefd[0] >= 0) {
                close(pipefd[0]);
            }
            if (pipefd[1] >= 0) {
                close(pipefd[1]);
            }
            if (prev_read >= 0) {
                close(prev_read);
            }

            if (cmds[i].builtin) {
                execute_child_builtin(shell, &cmds[i]);
            } else {
                execute_child_external(&cmds[i]);
            }
            _exit(1); /* should not reach */
        }

        pids[i] = pid;

        if (prev_read >= 0) {
            close(prev_read);
        }
        if (pipefd[1] >= 0) {
            close(pipefd[1]);
        }
        prev_read = pipefd[0];
    }

    if (prev_read >= 0) {
        close(prev_read);
    }

    for (size_t i = 0; i < count; ++i) {
        int wstatus = 0;
        if (waitpid(pids[i], &wstatus, 0) < 0) {
            status_result = GS_ERR_EXEC;
            continue;
        }
        if (i == count - 1) {
            if (WIFEXITED(wstatus)) {
                status_result = WEXITSTATUS(wstatus);
            } else if (WIFSIGNALED(wstatus)) {
                status_result = 128 + WTERMSIG(wstatus);
            }
        }
    }

cleanup:
    free(pids);
    return status_result;
}

static int execute_redir_only(gs_prepared_command *cmd) {
    saved_descriptor *saved = NULL;
    size_t saved_len = 0u;
    int rc = apply_parent_redirs(cmd->redirs, cmd->redir_count, &saved, &saved_len);
    restore_parent_redirs(saved, saved_len);
    if (rc != GS_OK) {
        return 1;
    }
    return 0;
}

int gs_execute_pipeline(struct gs_shell *shell, const gs_pipeline *pipeline) {
    if (!pipeline || pipeline->length == 0u) {
        return GS_OK;
    }

    if (pipeline->background) {
        fprintf(stderr, "genshell: background jobs are not implemented yet\n");
        shell->last_status = 1;
        return GS_ERR_UNIMPLEMENTED;
    }

    gs_prepared_command *prepared = (gs_prepared_command *)calloc(pipeline->length, sizeof(gs_prepared_command));
    if (!prepared) {
        return GS_ERR_ALLOC;
    }

    int rc = GS_OK;
    for (size_t i = 0; i < pipeline->length; ++i) {
        rc = prepare_command(shell, &pipeline->commands[i], &prepared[i]);
        if (rc != GS_OK) {
            goto done;
        }
    }

    int status = 0;

    if (pipeline->length == 1u && prepared[0].argc == 0u && prepared[0].redir_count > 0u) {
        status = execute_redir_only(&prepared[0]);
    } else if (pipeline->length == 1u && prepared[0].builtin && (prepared[0].builtin->flags & GS_BUILTIN_FLAG_PARENT)) {
        status = execute_parent_builtin(shell, &prepared[0]);
        if (status < 0) {
            status = 1;
        }
    } else {
        status = execute_pipeline_processes(shell, prepared, pipeline->length);
        if (status < 0) {
            status = 1;
        }
    }

    shell->last_status = status;

    rc = status;

done:
    for (size_t i = 0; i < pipeline->length; ++i) {
        dispose_prepared_command(&prepared[i]);
    }
    free(prepared);
    return rc;
}
