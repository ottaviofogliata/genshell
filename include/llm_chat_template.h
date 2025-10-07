#ifndef LLM_CHAT_TEMPLATE_H
#define LLM_CHAT_TEMPLATE_H

#include <stddef.h>

/*
 * The chat template interface is expressed as a tiny vtable so the CLI can
 * remain agnostic to the concrete LLM prompt wiring.  Each implementation
 * supplies a build callback that returns an allocated prompt string given the
 * desired system/user messages; callers free the returned buffer.  This mirrors
 * an object-oriented interface in plain C while keeping allocations owned by
 * the template module.
 */
struct llm_chat_template;

typedef char *(*llm_chat_template_build_fn)(const struct llm_chat_template *tmpl,
                                            const char *system_prompt,
                                            const char *user_prompt);

typedef void (*llm_chat_template_destroy_fn)(const struct llm_chat_template *tmpl);

struct llm_chat_template {
    llm_chat_template_build_fn build;
    llm_chat_template_destroy_fn destroy; /* optional; may be NULL */
    const void *ctx;                      /* implementation-defined payload */
};

char *llm_chat_template_build(const struct llm_chat_template *tmpl,
                              const char *system_prompt,
                              const char *user_prompt);

void llm_chat_template_release(const struct llm_chat_template *tmpl);

#endif /* LLM_CHAT_TEMPLATE_H */
