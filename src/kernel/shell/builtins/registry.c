#include "builtin.h"

#include <stdlib.h>
#include <string.h>

static int builtin_cd(struct gs_shell *shell, int argc, char *const argv[]);
static int builtin_exit(struct gs_shell *shell, int argc, char *const argv[]);
static int builtin_pwd(struct gs_shell *shell, int argc, char *const argv[]);
static int builtin_echo(struct gs_shell *shell, int argc, char *const argv[]);
static int builtin_export(struct gs_shell *shell, int argc, char *const argv[]);
static int builtin_unset(struct gs_shell *shell, int argc, char *const argv[]);
static int builtin_umask(struct gs_shell *shell, int argc, char *const argv[]);

static const gs_builtin_spec k_builtins[] = {
    {"cd", builtin_cd, GS_BUILTIN_FLAG_PARENT | GS_BUILTIN_FLAG_SPECIAL},
    {"echo", builtin_echo, 0u},
    {"exit", builtin_exit, GS_BUILTIN_FLAG_PARENT | GS_BUILTIN_FLAG_SPECIAL},
    {"export", builtin_export, GS_BUILTIN_FLAG_PARENT | GS_BUILTIN_FLAG_SPECIAL},
    {"pwd", builtin_pwd, GS_BUILTIN_FLAG_PARENT},
    {"umask", builtin_umask, GS_BUILTIN_FLAG_PARENT | GS_BUILTIN_FLAG_SPECIAL},
    {"unset", builtin_unset, GS_BUILTIN_FLAG_PARENT | GS_BUILTIN_FLAG_SPECIAL},
};

static int compare_builtin(const void *lhs, const void *rhs) {
    const gs_builtin_spec *a = (const gs_builtin_spec *)lhs;
    const gs_builtin_spec *b = (const gs_builtin_spec *)rhs;
    return strcmp(a->name, b->name);
}

const gs_builtin_spec *gs_builtin_lookup(const char *name) {
    if (!name) {
        return NULL;
    }
    gs_builtin_spec key = {name, NULL, 0u};
    const gs_builtin_spec *result = (const gs_builtin_spec *)bsearch(
        &key,
        k_builtins,
        sizeof(k_builtins) / sizeof(k_builtins[0]),
        sizeof(k_builtins[0]),
        compare_builtin);
    return result;
}

/* Declarations implemented in dedicated translation units. */
extern int genshell_builtin_cd(struct gs_shell *, int, char *const []);
extern int genshell_builtin_exit(struct gs_shell *, int, char *const []);
extern int genshell_builtin_pwd(struct gs_shell *, int, char *const []);
extern int genshell_builtin_echo(struct gs_shell *, int, char *const []);
extern int genshell_builtin_export(struct gs_shell *, int, char *const []);
extern int genshell_builtin_unset(struct gs_shell *, int, char *const []);
extern int genshell_builtin_umask(struct gs_shell *, int, char *const []);

static int builtin_cd(struct gs_shell *shell, int argc, char *const argv[]) {
    return genshell_builtin_cd(shell, argc, argv);
}

static int builtin_exit(struct gs_shell *shell, int argc, char *const argv[]) {
    return genshell_builtin_exit(shell, argc, argv);
}

static int builtin_pwd(struct gs_shell *shell, int argc, char *const argv[]) {
    return genshell_builtin_pwd(shell, argc, argv);
}

static int builtin_echo(struct gs_shell *shell, int argc, char *const argv[]) {
    return genshell_builtin_echo(shell, argc, argv);
}

static int builtin_export(struct gs_shell *shell, int argc, char *const argv[]) {
    return genshell_builtin_export(shell, argc, argv);
}

static int builtin_unset(struct gs_shell *shell, int argc, char *const argv[]) {
    return genshell_builtin_unset(shell, argc, argv);
}

static int builtin_umask(struct gs_shell *shell, int argc, char *const argv[]) {
    return genshell_builtin_umask(shell, argc, argv);
}
