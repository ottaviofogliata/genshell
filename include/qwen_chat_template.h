#ifndef QWEN_CHAT_TEMPLATE_H
#define QWEN_CHAT_TEMPLATE_H

#include "llm_chat_template.h"

/*
 * Factory for the Qwen-specific chat template.  The template encapsulates the
 * prompt layout used by Qwen2.5-style models so callers only depend on the
 * generic llm_chat_template contract.  Swapping models later is as simple as
 * returning a different implementation from this factory.
 */
const struct llm_chat_template *qwen_chat_template(void);

#endif /* QWEN_CHAT_TEMPLATE_H */
