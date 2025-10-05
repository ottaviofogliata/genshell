#ifndef GS_SHELL_H
#define GS_SHELL_H

#include <stdbool.h>
#include <stddef.h>

struct gs_pipeline;
struct gs_command_source;
struct gs_shell;

/*
 * Negative return codes map to internal errors.
 * Zero means success, positive values map to POSIX exit statuses.
 */
#define GS_OK 0
#define GS_ERR_ALLOC (-1)
#define GS_ERR_PARSE (-2)
#define GS_ERR_EOF (-3)
#define GS_ERR_EXEC (-4)
#define GS_ERR_UNIMPLEMENTED (-5)

/* Sentinel byte used to tag characters that must not be further expanded. */
#define GS_LITERAL_SENTINEL ((char)0x1D)

struct gs_shell {
    const char *progname;
    int last_status;
    bool exit_requested;
    int exit_status;
    bool interactive;
};

int gs_shell_init(struct gs_shell *shell, const char *progname);
void gs_shell_destroy(struct gs_shell *shell);
int gs_shell_run(struct gs_shell *shell);

#endif /* GS_SHELL_H */
