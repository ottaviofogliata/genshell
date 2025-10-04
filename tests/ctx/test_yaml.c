/*
 * Tests: validate the ctx_yaml parser handles scalar mappings, quoted values,
 * duplicate key errors, and file loading without external dependencies.
 */

#include "ctx_yaml.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define unlink _unlink
#else
#include <unistd.h>
#endif

/*
 * Helper that writes `content` to a temporary file and copies the resulting
 * path into `path_buf`. Exits the process if any error occurs.
 */
static void write_temp_file(const char *content, char *path_buf, size_t buf_size) {
    if (!content || !path_buf || buf_size == 0) {
        fprintf(stderr, "write_temp_file misuse\n");
        exit(EXIT_FAILURE);
    }

    const char *tmp_dir = getenv("CTX_YAML_TEST_TMPDIR");
    if (tmp_dir && *tmp_dir) {
        static unsigned long counter = 0;
        for (int attempts = 0; attempts < 10; ++attempts) {
            unsigned long id = counter++;
            if (snprintf(path_buf, buf_size, "%s/yaml_test_%lu.tmp", tmp_dir, id) >= (int) buf_size) {
                fprintf(stderr, "temporary path buffer too small\n");
                exit(EXIT_FAILURE);
            }
            FILE *fp = fopen(path_buf, "wb");
            if (!fp) {
                continue;
            }
            size_t written = fwrite(content, 1, strlen(content), fp);
            fclose(fp);
            if (written != strlen(content)) {
                unlink(path_buf);
                continue;
            }
            return;
        }
        fprintf(stderr, "failed to create temporary file in %s\n", tmp_dir);
        exit(EXIT_FAILURE);
    }

    char tmp_name[L_tmpnam];
    if (!tmpnam(tmp_name)) {
        fprintf(stderr, "tmpnam failed\n");
        exit(EXIT_FAILURE);
    }

    FILE *fp = fopen(tmp_name, "wb");
    if (!fp) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    size_t written = fwrite(content, 1, strlen(content), fp);
    fclose(fp);
    if (written != strlen(content)) {
        fprintf(stderr, "failed to write temporary file\n");
        unlink(tmp_name);
        exit(EXIT_FAILURE);
    }

    strncpy(path_buf, tmp_name, buf_size - 1);
    path_buf[buf_size - 1] = '\0';
    return;
}

/* Verifies simple key/value pairs parse and are retrievable via ctx_yaml_get. */
static void test_parse_simple_pairs(void) {
    const char *yaml = "first: alpha\n# comment\nsecond: beta\n";
    ctx_yaml_document doc;
    ctx_yaml_document_init(&doc);

    char errbuf[128];
    int rc = ctx_yaml_parse_string(yaml, &doc, errbuf, sizeof errbuf);
    assert(rc == 0);
    assert(doc.count == 2);
    assert(strcmp(ctx_yaml_get(&doc, "first"), "alpha") == 0);
    assert(strcmp(ctx_yaml_get(&doc, "second"), "beta") == 0);
    assert(ctx_yaml_get(&doc, "missing") == NULL);

    ctx_yaml_document_free(&doc);
}

/* Confirms that surrounding single/double quotes are stripped during parsing. */
static void test_parse_quoted_values(void) {
    const char *yaml = "quoted: \"value space\"\nalt: '42'\n";
    ctx_yaml_document doc;
    ctx_yaml_document_init(&doc);

    char errbuf[128];
    int rc = ctx_yaml_parse_string(yaml, &doc, errbuf, sizeof errbuf);
    assert(rc == 0);
    assert(doc.count == 2);
    assert(strcmp(ctx_yaml_get(&doc, "quoted"), "value space") == 0);
    assert(strcmp(ctx_yaml_get(&doc, "alt"), "42") == 0);

    ctx_yaml_document_free(&doc);
}

/* Ensures duplicate keys are rejected with a useful error message. */
static void test_duplicate_keys_fail(void) {
    const char *yaml = "dup: first\ndup: second\n";
    ctx_yaml_document doc;
    ctx_yaml_document_init(&doc);

    char errbuf[128];
    int rc = ctx_yaml_parse_string(yaml, &doc, errbuf, sizeof errbuf);
    assert(rc != 0);
    assert(strstr(errbuf, "duplicate") != NULL);

    ctx_yaml_document_free(&doc);
}

/* Verifies ctx_yaml_load_file reads disk content and parses it correctly. */
static void test_load_file_success(void) {
    const char *yaml = "path: /tmp/demo\nmode: read\n";
    char tmp_path[256];
    write_temp_file(yaml, tmp_path, sizeof tmp_path);

    ctx_yaml_document doc;
    ctx_yaml_document_init(&doc);

    char errbuf[128];
    int rc = ctx_yaml_load_file(tmp_path, &doc, errbuf, sizeof errbuf);
    assert(rc == 0);
    assert(doc.count == 2);
    assert(strcmp(ctx_yaml_get(&doc, "path"), "/tmp/demo") == 0);
    assert(strcmp(ctx_yaml_get(&doc, "mode"), "read") == 0);

    ctx_yaml_document_free(&doc);
    unlink(tmp_path);
}

int main(void) {
    test_parse_simple_pairs();
    test_parse_quoted_values();
    test_duplicate_keys_fail();
    test_load_file_success();
    return 0;
}
