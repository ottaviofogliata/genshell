#include "command_stream_parser.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static bool ensure_command_capacity(struct command_stream_parser *parser, size_t additional) {
    size_t required = parser->command_length + additional;
    if (required <= parser->command_capacity) {
        return true;
    }
    size_t new_capacity = parser->command_capacity == 0 ? 64 : parser->command_capacity;
    while (new_capacity < required) {
        new_capacity *= 2;
    }
    char *new_buffer = (char *)realloc(parser->command_buffer, new_capacity);
    if (!new_buffer) {
        parser->error = true;
        return false;
    }
    parser->command_buffer = new_buffer;
    parser->command_capacity = new_capacity;
    return true;
}

static bool command_buffer_append(struct command_stream_parser *parser, char ch) {
    if (!ensure_command_capacity(parser, 1)) {
        return false;
    }
    parser->command_buffer[parser->command_length++] = ch;
    return true;
}

static bool command_buffer_terminate(struct command_stream_parser *parser) {
    if (!ensure_command_capacity(parser, 1)) {
        return false;
    }
    parser->command_buffer[parser->command_length] = '\0';
    return true;
}

static void command_buffer_reset(struct command_stream_parser *parser) {
    parser->command_length = 0;
}

static bool ensure_raw_capacity(struct command_stream_parser *parser, size_t additional) {
    size_t required = parser->raw_length + additional;
    if (required <= parser->raw_capacity) {
        return true;
    }
    size_t new_capacity = parser->raw_capacity == 0 ? 256 : parser->raw_capacity;
    while (new_capacity < required) {
        new_capacity *= 2;
    }
    char *new_buffer = (char *)realloc(parser->raw_buffer, new_capacity);
    if (!new_buffer) {
        parser->error = true;
        return false;
    }
    parser->raw_buffer = new_buffer;
    parser->raw_capacity = new_capacity;
    return true;
}

static void raw_append(struct command_stream_parser *parser, const char *token) {
    if (!token) {
        return;
    }
    size_t len = strlen(token);
    if (len == 0) {
        return;
    }
    if (!ensure_raw_capacity(parser, len)) {
        return;
    }
    memcpy(parser->raw_buffer + parser->raw_length, token, len);
    parser->raw_length += len;
}

static void emit_command(struct command_stream_parser *parser) {
    if (!command_buffer_terminate(parser)) {
        return;
    }
    if (parser->callbacks.on_command) {
        parser->callbacks.on_command(parser->command_buffer, parser->user_data);
    }
    parser->commands_emitted = true;
    command_buffer_reset(parser);
}

void command_stream_parser_init(struct command_stream_parser *parser,
                                const struct command_stream_callbacks *callbacks,
                                void *user_data) {
    if (!parser) {
        return;
    }
    parser->command_buffer = NULL;
    parser->command_length = 0;
    parser->command_capacity = 0;
    parser->raw_buffer = NULL;
    parser->raw_length = 0;
    parser->raw_capacity = 0;
    parser->callbacks.on_command = NULL;
    parser->callbacks.on_empty = NULL;
    parser->callbacks.on_raw = NULL;
    if (callbacks) {
        parser->callbacks = *callbacks;
    }
    parser->user_data = user_data;
    command_stream_parser_reset(parser);
}

void command_stream_parser_reset(struct command_stream_parser *parser) {
    if (!parser) {
        return;
    }
    parser->state = COMMAND_STREAM_SEEK_KEY;
    parser->key_match_index = 0;
    parser->array_depth = 0;
    parser->capture_escape = false;
    parser->commands_emitted = false;
    parser->saw_commands_field = false;
    parser->finished = false;
    parser->error = false;
    parser->command_length = 0;
    parser->raw_length = 0;
}

void command_stream_parser_consume(struct command_stream_parser *parser, const char *token) {
    if (!parser || !token || parser->finished) {
        return;
    }

    raw_append(parser, token);

    static const char commands_key[] = "\"commands\"";
    const size_t key_len = sizeof(commands_key) - 1;

    for (const char *p = token; *p; ++p) {
        char ch = *p;
        switch (parser->state) {
            case COMMAND_STREAM_SEEK_KEY:
                if (ch == commands_key[parser->key_match_index]) {
                    parser->key_match_index++;
                    if (parser->key_match_index == key_len) {
                        parser->state = COMMAND_STREAM_SEEK_COLON;
                        parser->key_match_index = 0;
                        parser->saw_commands_field = true;
                    }
                } else {
                    parser->key_match_index = (ch == commands_key[0]) ? 1 : 0;
                }
                break;

            case COMMAND_STREAM_SEEK_COLON:
                if (ch == ':') {
                    parser->state = COMMAND_STREAM_SEEK_VALUE;
                } else if (!isspace((unsigned char)ch)) {
                    parser->state = COMMAND_STREAM_SEEK_KEY;
                    parser->key_match_index = 0;
                }
                break;

            case COMMAND_STREAM_SEEK_VALUE:
                if (isspace((unsigned char)ch)) {
                    break;
                }
                if (ch == '[') {
                    parser->state = COMMAND_STREAM_IN_ARRAY;
                    parser->array_depth = 1;
                } else if (ch == '"') {
                    parser->state = COMMAND_STREAM_IN_SINGLE_STRING;
                    parser->capture_escape = false;
                    command_buffer_reset(parser);
                } else {
                    parser->state = COMMAND_STREAM_SEEK_KEY;
                    parser->key_match_index = 0;
                }
                break;

            case COMMAND_STREAM_IN_ARRAY:
                if (ch == '[') {
                    parser->array_depth++;
                } else if (ch == ']') {
                    parser->array_depth--;
                    if (parser->array_depth <= 0) {
                        parser->state = COMMAND_STREAM_DONE;
                    }
                } else if (parser->array_depth == 1 && ch == '"') {
                    parser->state = COMMAND_STREAM_IN_ARRAY_STRING;
                    parser->capture_escape = false;
                    command_buffer_reset(parser);
                }
                break;

            case COMMAND_STREAM_IN_ARRAY_STRING:
                if (parser->capture_escape) {
                    if (!command_buffer_append(parser, ch)) {
                        parser->state = COMMAND_STREAM_DONE;
                    }
                    parser->capture_escape = false;
                } else if (ch == '\\') {
                    parser->capture_escape = true;
                } else if (ch == '"') {
                    emit_command(parser);
                    parser->state = COMMAND_STREAM_IN_ARRAY;
                } else {
                    if (!command_buffer_append(parser, ch)) {
                        parser->state = COMMAND_STREAM_DONE;
                    }
                }
                break;

            case COMMAND_STREAM_IN_SINGLE_STRING:
                if (parser->capture_escape) {
                    if (!command_buffer_append(parser, ch)) {
                        parser->state = COMMAND_STREAM_DONE;
                    }
                    parser->capture_escape = false;
                } else if (ch == '\\') {
                    parser->capture_escape = true;
                } else if (ch == '"') {
                    emit_command(parser);
                    parser->state = COMMAND_STREAM_DONE;
                } else {
                    if (!command_buffer_append(parser, ch)) {
                        parser->state = COMMAND_STREAM_DONE;
                    }
                }
                break;

            case COMMAND_STREAM_DONE:
            default:
                break;
        }
    }
}

void command_stream_parser_finish(struct command_stream_parser *parser) {
    if (!parser || parser->finished) {
        return;
    }
    parser->finished = true;

    if (parser->raw_length > 0 && ensure_raw_capacity(parser, 1)) {
        parser->raw_buffer[parser->raw_length] = '\0';
    }

    if (parser->error) {
        if (parser->callbacks.on_raw && parser->raw_buffer) {
            parser->callbacks.on_raw(parser->raw_buffer, parser->user_data);
        }
        return;
    }

    if (parser->commands_emitted) {
        return;
    }

    if (parser->saw_commands_field) {
        if (parser->callbacks.on_empty) {
            parser->callbacks.on_empty(parser->user_data);
        }
        return;
    }

    if (parser->callbacks.on_raw && parser->raw_buffer && parser->raw_length > 0) {
        parser->callbacks.on_raw(parser->raw_buffer, parser->user_data);
    }
}

void command_stream_parser_free(struct command_stream_parser *parser) {
    if (!parser) {
        return;
    }
    free(parser->command_buffer);
    free(parser->raw_buffer);
    parser->command_buffer = NULL;
    parser->raw_buffer = NULL;
    parser->command_capacity = 0;
    parser->raw_capacity = 0;
    parser->command_length = 0;
    parser->raw_length = 0;
}
