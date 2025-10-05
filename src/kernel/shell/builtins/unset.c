/*
 * unset - POSIX shell builtin
 * Removes variables and functions from the execution environment. This
 * implementation focuses on variable removal via unsetenv(3) and validates
 * identifiers before attempting to erase them.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "builtin.h"

static int is_valid_name(const char *name) {
    if (!name || name[0] == '\0') {
        return 0;
    }
    if (!(isalpha((unsigned char)name[0]) || name[0] == '_')) {
        return 0;
    }
    for (const char *p = name + 1; *p; ++p) {
        if (!(isalnum((unsigned char)*p) || *p == '_')) {
            return 0;
        }
    }
    return 1;
}

int genshell_builtin_unset(struct gs_shell *shell, int argc, char *const argv[]) {
    (void)shell;

    if (argc < 2) {
        return 0;
    }

    int status = 0;

    for (int i = 1; i < argc; ++i) {
        if (!is_valid_name(argv[i])) {
            fprintf(stderr, "genshell: unset: `%s': invalid identifier\n", argv[i]);
            status = 1;
            continue;
        }
        if (unsetenv(argv[i]) != 0) {
            fprintf(stderr, "genshell: unset: failed to unset %s\n", argv[i]);
            status = 1;
        }
    }

    return status;
}
