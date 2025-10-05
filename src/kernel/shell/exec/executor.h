#ifndef GS_EXECUTOR_H
#define GS_EXECUTOR_H

#include "../parser/ast.h"
#include "../shell.h"

int gs_execute_pipeline(struct gs_shell *shell, const gs_pipeline *pipeline);

#endif /* GS_EXECUTOR_H */
