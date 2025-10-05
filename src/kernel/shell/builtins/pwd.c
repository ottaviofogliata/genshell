/*
 * pwd - POSIX shell builtin
 * Prints the shell's current working directory. Supports -L (logical PWD) and
 * -P (physical path via getcwd), defaulting to logical semantics when possible.
 */

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"

int genshell_builtin_pwd(struct gs_shell *shell, int argc, char *const argv[]) {
    (void)shell;

    bool logical = true;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-L") == 0) {
            logical = true;
        } else if (strcmp(argv[i], "-P") == 0) {
            logical = false;
        } else {
            fprintf(stderr, "genshell: pwd: invalid option -- %s\n", argv[i]);
            return 1;
        }
    }

    const char *pwd = NULL;
    char buf[PATH_MAX];

    if (logical) {
        pwd = getenv("PWD");
    }

    if (!pwd || pwd[0] == '\0' || !logical) {
        if (getcwd(buf, sizeof(buf)) == NULL) {
            fprintf(stderr, "genshell: pwd: %s\n", strerror(errno));
            return 1;
        }
        pwd = buf;
    }

    printf("%s\n", pwd);
    fflush(stdout);
    return 0;
}
