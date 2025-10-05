#include "shell.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    (void)argc;

    struct gs_shell shell;
    if (gs_shell_init(&shell, argv ? argv[0] : "genshell") != GS_OK) {
        fputs("genshell: failed to initialise shell\n", stderr);
        return EXIT_FAILURE;
    }

    int status = gs_shell_run(&shell);
    gs_shell_destroy(&shell);
    if (status < 0) {
        status = EXIT_FAILURE;
    }
    return status;
}
