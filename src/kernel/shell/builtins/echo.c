/*
 * echo - POSIX shell builtin
 * Writes its arguments separated by spaces followed by a newline. Supports the
 * common "-n" option to suppress the trailing newline. Behaviour for other
 * flags matches the simple POSIX baseline (no escape processing).
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "builtin.h"

int genshell_builtin_echo(struct gs_shell *shell, int argc, char *const argv[]) {
    (void)shell;

    int start = 1;
    bool newline = true;

    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        newline = false;
        start = 2;
    }

    for (int i = start; i < argc; ++i) {
        fputs(argv[i], stdout);
        if (i + 1 < argc) {
            fputc(' ', stdout);
        }
    }

    if (newline) {
        fputc('\n', stdout);
    }
    fflush(stdout);
    return 0;
}
