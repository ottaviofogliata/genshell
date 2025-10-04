#include "ctx_yaml.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ctx_yaml_set_error(char *errbuf, size_t errbuf_size, const char *message, size_t line) {
    if (!errbuf || errbuf_size == 0) {
        return;
    }

    if (line > 0) {
        snprintf(errbuf, errbuf_size, "line %zu: %s", line, message);
    } else {
        snprintf(errbuf, errbuf_size, "%s", message);
    }
}

static char *ctx_yaml_strdup(const char *src) {
    if (!src) {
        return NULL;
    }
    size_t len = strlen(src);
    char *copy = (char *) malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, src, len + 1);
    return copy;
}

static char *ctx_yaml_trim(char *text) {
    if (!text) {
        return NULL;
    }

    while (*text && isspace((unsigned char) *text)) {
        ++text;
    }

    if (*text == '\0') {
        return text;
    }

    char *end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char) *end)) {
        *end-- = '\0';
    }
    return text;
}

static void ctx_yaml_document_reset(ctx_yaml_document *doc) {
    if (!doc) {
        return;
    }
    doc->keys = NULL;
    doc->values = NULL;
    doc->count = 0;
}

void ctx_yaml_document_init(ctx_yaml_document *doc) {
    ctx_yaml_document_reset(doc);
}

void ctx_yaml_document_free(ctx_yaml_document *doc) {
    if (!doc) {
        return;
    }

    if (doc->keys) {
        for (size_t i = 0; i < doc->count; ++i) {
            free(doc->keys[i]);
        }
        free(doc->keys);
    }
    if (doc->values) {
        for (size_t i = 0; i < doc->count; ++i) {
            free(doc->values[i]);
        }
        free(doc->values);
    }
    ctx_yaml_document_reset(doc);
}

static int ctx_yaml_document_append(ctx_yaml_document *doc, const char *key, const char *value) {
    if (!doc) {
        return -1;
    }

    char **new_keys = (char **) realloc(doc->keys, (doc->count + 1) * sizeof(char *));
    if (!new_keys) {
        return -1;
    }
    doc->keys = new_keys;

    char **new_values = (char **) realloc(doc->values, (doc->count + 1) * sizeof(char *));
    if (!new_values) {
        return -1;
    }
    doc->values = new_values;

    doc->keys[doc->count] = NULL;
    doc->values[doc->count] = NULL;

    char *key_copy = ctx_yaml_strdup(key);
    if (!key_copy) {
        return -1;
    }
    doc->keys[doc->count] = key_copy;

    char *value_copy = ctx_yaml_strdup(value);
    if (!value_copy) {
        free(doc->keys[doc->count]);
        doc->keys[doc->count] = NULL;
        return -1;
    }
    doc->values[doc->count] = value_copy;

    doc->count += 1;
    return 0;
}

static void ctx_yaml_unquote(char *text) {
    if (!text) {
        return;
    }
    size_t len = strlen(text);
    if (len < 2) {
        return;
    }
    char first = text[0];
    char last = text[len - 1];
    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
        memmove(text, text + 1, len - 2);
        text[len - 2] = '\0';
    }
}

int ctx_yaml_parse_string(const char *input,
                          ctx_yaml_document *doc,
                          char *errbuf,
                          size_t errbuf_size) {
    if (!doc) {
        ctx_yaml_set_error(errbuf, errbuf_size, "document pointer is null", 0);
        return -1;
    }
    ctx_yaml_document_free(doc);

    if (!input) {
        ctx_yaml_set_error(errbuf, errbuf_size, "input string is null", 0);
        return -1;
    }

    size_t line_number = 0;
    const char *cursor = input;

    while (*cursor) {
        const char *line_start = cursor;
        while (*cursor && *cursor != '\n' && *cursor != '\r') {
            ++cursor;
        }
        size_t line_length = (size_t)(cursor - line_start);
        if (*cursor == '\r' && *(cursor + 1) == '\n') {
            cursor += 2;
        } else if (*cursor == '\n' || *cursor == '\r') {
            ++cursor;
        }

        line_number++;
        char *line = (char *) malloc(line_length + 1);
        if (!line) {
            ctx_yaml_set_error(errbuf, errbuf_size, "out of memory", line_number);
            ctx_yaml_document_free(doc);
            return -1;
        }
        memcpy(line, line_start, line_length);
        line[line_length] = '\0';

        char *trimmed = ctx_yaml_trim(line);
        if (*trimmed == '\0' || *trimmed == '#') {
            free(line);
            continue;
        }

        char *colon = strchr(trimmed, ':');
        if (!colon) {
            ctx_yaml_set_error(errbuf, errbuf_size, "missing ':' delimiter", line_number);
            free(line);
            ctx_yaml_document_free(doc);
            return -1;
        }

        *colon = '\0';
        char *key = ctx_yaml_trim(trimmed);
        char *value = ctx_yaml_trim(colon + 1);

        if (*key == '\0') {
            ctx_yaml_set_error(errbuf, errbuf_size, "empty key", line_number);
            free(line);
            ctx_yaml_document_free(doc);
            return -1;
        }

        ctx_yaml_unquote(value);

        for (size_t i = 0; i < doc->count; ++i) {
            if (strcmp(doc->keys[i], key) == 0) {
                ctx_yaml_set_error(errbuf, errbuf_size, "duplicate key", line_number);
                free(line);
                ctx_yaml_document_free(doc);
                return -1;
            }
        }

        if (ctx_yaml_document_append(doc, key, value) != 0) {
            ctx_yaml_set_error(errbuf, errbuf_size, strerror(errno ? errno : ENOMEM), line_number);
            free(line);
            ctx_yaml_document_free(doc);
            return -1;
        }

        free(line);
    }

    return 0;
}

int ctx_yaml_load_file(const char *path,
                       ctx_yaml_document *doc,
                       char *errbuf,
                       size_t errbuf_size) {
    if (!path) {
        ctx_yaml_set_error(errbuf, errbuf_size, "path is null", 0);
        return -1;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        ctx_yaml_set_error(errbuf, errbuf_size, strerror(errno), 0);
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        ctx_yaml_set_error(errbuf, errbuf_size, strerror(errno), 0);
        fclose(fp);
        return -1;
    }
    long length = ftell(fp);
    if (length < 0) {
        ctx_yaml_set_error(errbuf, errbuf_size, strerror(errno), 0);
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        ctx_yaml_set_error(errbuf, errbuf_size, strerror(errno), 0);
        fclose(fp);
        return -1;
    }

    char *buffer = (char *) malloc((size_t) length + 1);
    if (!buffer) {
        ctx_yaml_set_error(errbuf, errbuf_size, "out of memory", 0);
        fclose(fp);
        return -1;
    }

    size_t read_bytes = fread(buffer, 1, (size_t) length, fp);
    fclose(fp);
    if (read_bytes != (size_t) length) {
        ctx_yaml_set_error(errbuf, errbuf_size, "failed to read file", 0);
        free(buffer);
        return -1;
    }
    buffer[length] = '\0';

    int rc = ctx_yaml_parse_string(buffer, doc, errbuf, errbuf_size);
    free(buffer);
    return rc;
}

const char *ctx_yaml_get(const ctx_yaml_document *doc, const char *key) {
    if (!doc || !key) {
        return NULL;
    }
    for (size_t i = 0; i < doc->count; ++i) {
        if (strcmp(doc->keys[i], key) == 0) {
            return doc->values[i];
        }
    }
    return NULL;
}
