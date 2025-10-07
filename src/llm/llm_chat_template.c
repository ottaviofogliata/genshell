#include "llm_chat_template.h"

char *llm_chat_template_build(const struct llm_chat_template *tmpl,
                              const char *system_prompt,
                              const char *user_prompt) {
    if (!tmpl || !tmpl->build) {
        return NULL;
    }
    return tmpl->build(tmpl, system_prompt, user_prompt);
}

void llm_chat_template_release(const struct llm_chat_template *tmpl) {
    if (!tmpl || !tmpl->destroy) {
        return;
    }
    tmpl->destroy(tmpl);
}
