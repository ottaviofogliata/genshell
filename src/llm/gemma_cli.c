#include "command_stream_parser.h"
#include "gemma_runner.h"
#include "llm_chat_template.h"
#include "qwen_chat_template.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    struct command_stream_parser parser;
    size_t tokens_emitted;
    double start_seconds;
    FILE *stream;
} cli_session;

static const char kDefaultModelPath[] = "models/qwen2.5-coder-3b-instruct-q4_k_m.gguf";
static const char kSystemPrompt[] =
    "You are a command-line planning assistant. Given the human message, respond "
    "with minified JSON that contains a single key named \"commands\". "
    "\"commands\" must always be an array of shell command strings in execution order. "
    "Do not include explanations, comments, or additional keys. If no command is needed, "
    "return an empty array.";

static double monotonic_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void cli_on_command(const char *command, void *user_data) {
    cli_session *session = (cli_session *)user_data;
    if (!session || !command) {
        return;
    }
    fprintf(session->stream, "%s\n", command);
    fflush(session->stream);
}

static void cli_on_empty(void *user_data) {
    cli_session *session = (cli_session *)user_data;
    if (!session) {
        return;
    }
    fputs("[]\n", session->stream);
    fflush(session->stream);
}

static void cli_on_raw(const char *raw_json, void *user_data) {
    cli_session *session = (cli_session *)user_data;
    if (!session || !raw_json) {
        return;
    }
    fputs(raw_json, session->stream);
    if (raw_json[0] != '\0' && raw_json[strlen(raw_json) - 1] != '\n') {
        fputc('\n', session->stream);
    }
    fflush(session->stream);
}

static int cli_token_callback(const char *token, void *user_data) {
    cli_session *session = (cli_session *)user_data;
    if (!session) {
        return 0;
    }

    if (!token) {
        command_stream_parser_finish(&session->parser);
        return 0;
    }

    session->tokens_emitted++;
    command_stream_parser_consume(&session->parser, token);
    return 1;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <model.gguf> [prompt]\n", prog);
}

int main(int argc, char **argv) {
    const char *model_path = kDefaultModelPath;
    const char *user_prompt = NULL;

    if (argc > 1) {
        model_path = argv[1];
    }
    if (argc > 2) {
        user_prompt = argv[2];
    }

    if (!user_prompt) {
        usage(argv[0]);
        fprintf(stderr, "Falling back to default prompt.\n");
        user_prompt = "Write a haiku about shell automation.";
    }

    /*
     * Chat template selection is routed through an interface so the CLI can
     * swap LLM-specific formats without touching the streaming or runtime
     * layers.  Today we return the Qwen template, but the design lets us wire a
     * different model by changing only this factory call.
     */
    const struct llm_chat_template *template = qwen_chat_template();
    char *chat_prompt = llm_chat_template_build(template, kSystemPrompt, user_prompt);
    if (!chat_prompt) {
        fprintf(stderr, "Failed to build chat prompt.\n");
        llm_chat_template_release(template);
        return EXIT_FAILURE;
    }

    struct gemma_runner runner;
    char errbuf[512];
    if (gemma_runner_init(&runner, model_path, errbuf, sizeof errbuf) != 0) {
        fprintf(stderr, "Failed to initialize model: %s\n", errbuf);
        free(chat_prompt);
        llm_chat_template_release(template);
        return EXIT_FAILURE;
    }

    cli_session session = {
        .tokens_emitted = 0,
        .start_seconds = monotonic_seconds(),
        .stream = stdout,
    };

    struct command_stream_callbacks callbacks = {
        .on_command = cli_on_command,
        .on_empty = cli_on_empty,
        .on_raw = cli_on_raw,
    };
    command_stream_parser_init(&session.parser, &callbacks, &session);

    fprintf(session.stream, "Model: %s\nUser: %s\n---\n", model_path, user_prompt);
    int rc = gemma_runner_generate(&runner, chat_prompt, cli_token_callback, &session, errbuf, sizeof errbuf);
    command_stream_parser_finish(&session.parser);

    if (rc != 0) {
        fprintf(stderr, "\nGeneration error: %s (rc=%d)\n", errbuf, rc);
    } else {
        fputc('\n', session.stream);
        double elapsed = monotonic_seconds() - session.start_seconds;
        double tps = elapsed > 0.0 ? session.tokens_emitted / elapsed : 0.0;
        fprintf(session.stream,
                "Generated %zu tokens in %.2f s (%.2f tok/s)\n",
                session.tokens_emitted,
                elapsed,
                tps);
    }

    command_stream_parser_free(&session.parser);
    gemma_runner_destroy(&runner);
    free(chat_prompt);
    llm_chat_template_release(template);
    return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
