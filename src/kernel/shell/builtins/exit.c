/*
 * exit - POSIX shell builtin
 * Terminates the current shell session. Accepts an optional numeric status
 * argument; if omitted, the shell exits with its last command status. When the
 * argument is not a valid integer, the shell exits with status 2 after reporting
 * the error, matching the traditional POSIX behaviour.
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "builtin.h"

int genshell_builtin_exit(struct gs_shell *shell, int argc, char *const argv[]) {
    if (!shell) {
        return GS_ERR_EXEC;
    }

    if (argc > 2) {
        fprintf(stderr, "genshell: exit: too many arguments\n");
        return 1;
    }

    int exit_code = shell->last_status;

    if (argc == 2) {
        errno = 0;
        char *end = NULL;
        long value = strtol(argv[1], &end, 10);
        if (errno != 0 || !end || *end != '\0') {
            fprintf(stderr, "genshell: exit: %s: numeric argument required\n", argv[1]);
            shell->exit_requested = true;
            shell->exit_status = 2;
            return 2;
        }
        exit_code = (int)(value & 0xFF);
    }

    shell->exit_requested = true;
    shell->exit_status = exit_code;
    return exit_code;
}
