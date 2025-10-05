/*
 * cd - POSIX shell builtin
 * Changes the shell's current working directory, updating PWD/OLDPWD and
 * supporting the usual forms: "cd" (HOME), "cd -" (previous directory), and
 * explicit path arguments. The command runs in the parent shell process so the
 * directory change persists for subsequent commands.
 */

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"

int genshell_builtin_cd(struct gs_shell *shell, int argc, char *const argv[]) {
    (void)shell;

    const char *target = NULL;
    int status = 0;
    char old_pwd[PATH_MAX];
    bool have_old = false;

    if (getcwd(old_pwd, sizeof(old_pwd)) != NULL) {
        have_old = true;
    }

    if (argc < 2) {
        target = getenv("HOME");
        if (!target || target[0] == '\0') {
            fprintf(stderr, "genshell: cd: HOME not set\n");
            return 1;
        }
    } else if (argc > 2) {
        fprintf(stderr, "genshell: cd: too many arguments\n");
        return 1;
    } else if (strcmp(argv[1], "-") == 0) {
        target = getenv("OLDPWD");
        if (!target || target[0] == '\0') {
            fprintf(stderr, "genshell: cd: OLDPWD not set\n");
            return 1;
        }
    } else {
        target = argv[1];
    }

    if (chdir(target) != 0) {
        fprintf(stderr, "genshell: cd: %s: %s\n", target, strerror(errno));
        return 1;
    }

    char new_pwd[PATH_MAX];
    if (getcwd(new_pwd, sizeof(new_pwd)) == NULL) {
        /* Fall back to the target path if getcwd fails (e.g., perms issues). */
        strncpy(new_pwd, target, sizeof(new_pwd) - 1u);
        new_pwd[sizeof(new_pwd) - 1u] = '\0';
    }

    if (have_old) {
        setenv("OLDPWD", old_pwd, 1);
    }
    setenv("PWD", new_pwd, 1);

    if (argc == 2 && strcmp(argv[1], "-") == 0) {
        printf("%s\n", new_pwd);
        fflush(stdout);
    }

    return status;
}
