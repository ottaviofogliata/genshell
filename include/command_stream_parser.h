#ifndef COMMAND_STREAM_PARSER_H
#define COMMAND_STREAM_PARSER_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Streaming JSON parser for the `"commands"` field emitted by the LLM helper.
 * The parser keeps just enough state to spot the commands array (or a single
 * string) and emits callbacks as tokens arrive.  Callers can treat this as an
 * observer pattern: we notify when a command is ready, when the commands field
 * is empty, or when we have to fall back to the raw JSON payload.
 */
struct command_stream_callbacks {
    void (*on_command)(const char *command, void *user_data);
    void (*on_empty)(void *user_data);
    void (*on_raw)(const char *raw_json, void *user_data);
};

enum command_stream_state {
    COMMAND_STREAM_SEEK_KEY = 0,
    COMMAND_STREAM_SEEK_COLON,
    COMMAND_STREAM_SEEK_VALUE,
    COMMAND_STREAM_IN_ARRAY,
    COMMAND_STREAM_IN_ARRAY_STRING,
    COMMAND_STREAM_IN_SINGLE_STRING,
    COMMAND_STREAM_DONE
};

struct command_stream_parser {
    enum command_stream_state state;
    size_t key_match_index;
    int array_depth;
    bool capture_escape;
    bool commands_emitted;
    bool saw_commands_field;
    bool finished;
    bool error;

    char *command_buffer;
    size_t command_length;
    size_t command_capacity;

    char *raw_buffer;
    size_t raw_length;
    size_t raw_capacity;

    struct command_stream_callbacks callbacks;
    void *user_data;
};

void command_stream_parser_init(struct command_stream_parser *parser,
                                const struct command_stream_callbacks *callbacks,
                                void *user_data);

void command_stream_parser_reset(struct command_stream_parser *parser);

void command_stream_parser_consume(struct command_stream_parser *parser, const char *token);

void command_stream_parser_finish(struct command_stream_parser *parser);

void command_stream_parser_free(struct command_stream_parser *parser);

#endif /* COMMAND_STREAM_PARSER_H */
