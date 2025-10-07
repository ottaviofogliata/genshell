#include "qwen_chat_template.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Qwen chat template implementation.  The layout mirrors the llama.cpp
 * metadata for Qwen2.5-style instruction checkpoints:
 *   <system>
 *   <user>
 *   assistant prefix (no content yet)
 */
static char *qwen_template_build(const struct llm_chat_template *tmpl,
                                 const char *system_prompt,
                                 const char *user_prompt) {
    (void)tmpl;
    if (!system_prompt || !user_prompt) {
        return NULL;
    }

    static const char system_prefix[] = "<|im_start|>system\n";
    static const char system_suffix[] = "<|im_end|>\n";
    static const char user_prefix[] = "<|im_start|>user\n";
    static const char user_suffix[] = "<|im_end|>\n";
    static const char assistant_prefix[] = "<|im_start|>assistant\n";

    size_t total_len = sizeof(system_prefix) - 1 + strlen(system_prompt) + sizeof(system_suffix) - 1 +
                       sizeof(user_prefix) - 1 + strlen(user_prompt) + sizeof(user_suffix) - 1 +
                       sizeof(assistant_prefix) - 1 + 1;

    char *buffer = (char *)malloc(total_len);
    if (!buffer) {
        return NULL;
    }

    int written = snprintf(buffer,
                           total_len,
                           "%s%s%s%s%s%s%s",
                           system_prefix,
                           system_prompt,
                           system_suffix,
                           user_prefix,
                           user_prompt,
                           user_suffix,
                           assistant_prefix);
    if (written < 0 || (size_t)written >= total_len) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

static const struct llm_chat_template kQwenTemplate = {
    .build = qwen_template_build,
    .destroy = NULL,
    .ctx = NULL,
};

const struct llm_chat_template *qwen_chat_template(void) {
    return &kQwenTemplate;
}
