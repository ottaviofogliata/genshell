#ifndef GS_BUILTIN_H
#define GS_BUILTIN_H

#include "../shell.h"

#define GS_BUILTIN_FLAG_PARENT 0x01u
#define GS_BUILTIN_FLAG_SPECIAL 0x02u

typedef int (*gs_builtin_fn)(struct gs_shell *shell, int argc, char *const argv[]);

typedef struct {
    const char *name;
    gs_builtin_fn fn;
    unsigned flags;
} gs_builtin_spec;

const gs_builtin_spec *gs_builtin_lookup(const char *name);

#endif /* GS_BUILTIN_H */
