#ifndef CTX_YAML_H
#define CTX_YAML_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal YAML document representation for top-level key/value pairs.
 * Keys and values are stored as null-terminated UTF-8 strings owned by the
 * document; callers must call ctx_yaml_document_free() when finished.
 */
typedef struct {
    char **keys;
    char **values;
    size_t count;
} ctx_yaml_document;

/* Initializes an empty YAML document. Useful before reuse between parses. */
void ctx_yaml_document_init(ctx_yaml_document *doc);

/* Releases all memory owned by the document and resets it to an empty state. */
void ctx_yaml_document_free(ctx_yaml_document *doc);

/*
 * Parses a YAML string containing a single mapping of scalar values.
 * Supported grammar:
 *   - Empty lines and comments (starting with '#') are ignored.
 *   - Each data line contains `key: value` at the top level (no nesting).
 *   - Values may be wrapped in single or double quotes.
 * Returns 0 on success; negative value on error with errbuf populated.
 */
int ctx_yaml_parse_string(const char *input,
                          ctx_yaml_document *doc,
                          char *errbuf,
                          size_t errbuf_size);

/* Loads a YAML file from disk and parses it using ctx_yaml_parse_string. */
int ctx_yaml_load_file(const char *path,
                       ctx_yaml_document *doc,
                       char *errbuf,
                       size_t errbuf_size);

/* Retrieves the value for `key`; returns NULL when the key is absent. */
const char *ctx_yaml_get(const ctx_yaml_document *doc, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* CTX_YAML_H */
