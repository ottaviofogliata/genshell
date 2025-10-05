/*
 * umask - POSIX shell builtin
 * Displays or updates the shell's file-creation mask. Accepts an optional
 * octal mask argument (e.g., 022). With -S it prints the symbolic rwx form of
 * the current mask. Any changes persist for subsequent child processes.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "builtin.h"

static void print_symbolic(mode_t mask) {
    const char classes[3] = {'u', 'g', 'o'};
    const mode_t bits[3] = {S_IRWXU, S_IRWXG, S_IRWXO};

    printf("%#03o", (unsigned)(mask & 0777));
    for (int i = 0; i < 3; ++i) {
        printf(" %c=", classes[i]);
        mode_t class_bits = (~mask) & bits[i];
        if (class_bits & (S_IRUSR >> (i * 3))) {
            putchar('r');
        }
        if (class_bits & (S_IWUSR >> (i * 3))) {
            putchar('w');
        }
        if (class_bits & (S_IXUSR >> (i * 3))) {
            putchar('x');
        }
        putchar(' ');
    }
    putchar('\n');
}

static void print_octal(mode_t mask) {
    printf("%03o\n", (unsigned)(mask & 0777));
}

int genshell_builtin_umask(struct gs_shell *shell, int argc, char *const argv[]) {
    (void)shell;

    int argi = 1;
    int symbolic = 0;

    if (argc > 1 && strcmp(argv[1], "-S") == 0) {
        symbolic = 1;
        argi = 2;
    }

    if (argc == argi) {
        mode_t mask = umask(0);
        umask(mask);
        if (symbolic) {
            print_symbolic(mask);
        } else {
            print_octal(mask);
        }
        return 0;
    }

    if (argc > argi + 1) {
        fprintf(stderr, "genshell: umask: too many arguments\n");
        return 1;
    }

    const char *arg = argv[argi];
    errno = 0;
    char *end = NULL;
    long value = strtol(arg, &end, 8);
    if (errno != 0 || !end || *end != '\0' || value < 0 || value > 0777) {
        fprintf(stderr, "genshell: umask: invalid mode: %s\n", arg);
        return 1;
    }

    umask((mode_t)value);
    return 0;
}
