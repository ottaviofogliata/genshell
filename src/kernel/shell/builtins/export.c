/*
 * export - POSIX shell builtin
 * Marks shell variables for export to the environment of subsequent commands.
 * Without operands it lists the current exported variables. With NAME=VALUE
 * pairs it assigns and exports, and with bare names it promotes existing shell
 * variables (or initialises them empty) into the environment.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtin.h"

extern char **environ;

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

static void print_exports(void) {
    for (char **env = environ; env && *env; ++env) {
        const char *entry = *env;
        const char *eq = strchr(entry, '=');
        if (!eq) {
            continue;
        }
        printf("export %.*s=%s\n", (int)(eq - entry), entry, eq + 1);
    }
}

int genshell_builtin_export(struct gs_shell *shell, int argc, char *const argv[]) {
    (void)shell;

    if (argc == 1) {
        print_exports();
        return 0;
    }

    int status = 0;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *eq = strchr(arg, '=');
        if (eq) {
            size_t name_len = (size_t)(eq - arg);
            char *name = strndup(arg, name_len);
            if (!name) {
                fprintf(stderr, "genshell: export: allocation failure\n");
                return 1;
            }
            if (!is_valid_name(name)) {
                fprintf(stderr, "genshell: export: `%s': invalid identifier\n", arg);
                free(name);
                status = 1;
                continue;
            }
            const char *value = eq + 1;
            if (setenv(name, value, 1) != 0) {
                fprintf(stderr, "genshell: export: failed to set %s\n", name);
                status = 1;
            }
            free(name);
        } else {
            if (!is_valid_name(arg)) {
                fprintf(stderr, "genshell: export: `%s': invalid identifier\n", arg);
                status = 1;
                continue;
            }
            const char *value = getenv(arg);
            if (!value) {
                value = "";
            }
            if (setenv(arg, value, 1) != 0) {
                fprintf(stderr, "genshell: export: failed to export %s\n", arg);
                status = 1;
            }
        }
    }

    return status;
}
