#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "secsgml.h"
#include "standardize_submission_metadata.h"

#ifdef _WIN32
#include <direct.h>
static int make_dir(const char *path) {
    if (_mkdir(path) == 0) return 0;
    if (errno == EEXIST) return 0;
    return -1;
}
#else
#include <sys/stat.h>
static int make_dir(const char *path) {
    if (mkdir(path, 0755) == 0) return 0;
    if (errno == EEXIST) return 0;
    return -1;
}
#endif

static uint8_t *load_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size_t len = (size_t)ftell(f);
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc(len ? len : 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, len, f);
    fclose(f);
    if (out_len) *out_len = len;
    return buf;
}

// Timing
#ifdef _WIN32
#include <windows.h>
static double now_ms(void) {
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
}
#else
#include <time.h>
static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}
#endif

static void write_span(FILE *f, byte_span s) {
    if (s.ptr && s.len > 0) {
        fwrite(s.ptr, 1, s.len, f);
    }
}

static int key_eq(byte_span a, byte_span b) {
    if (a.len != b.len) return 0;
    if (a.len == 0) return 1;
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

static void write_json_string(FILE *f, byte_span s) {
    fputc('"', f);
    for (size_t i = 0; i < s.len; i++) {
        unsigned char c = s.ptr[i];
        if (c == '"' || c == '\\' || c < 0x20 || c >= 0x80) {
            fprintf(f, "\\u%04X", (unsigned)c);
        } else {
            fputc((int)c, f);
        }
    }
    fputc('"', f);
}

static size_t find_section_end(const submission_event *events, size_t count, size_t start_idx) {
    int depth = events[start_idx].depth;
    for (size_t i = start_idx + 1; i < count; i++) {
        if (events[i].type == SUB_EVENT_SECTION_END && events[i].depth == depth) {
            return i;
        }
    }
    return count;
}

typedef struct {
    byte_span key;
    submission_event_type type;
    size_t idx;
    size_t end_idx;
} json_member;

static void write_object_range(FILE *f, const submission_event *events, size_t start_idx, size_t end_idx, int depth) {
    json_member *members = NULL;
    size_t member_count = 0;

    byte_span *uniq_keys = NULL;
    size_t *uniq_counts = NULL;
    size_t uniq_count = 0;

    for (size_t i = start_idx; i < end_idx; i++) {
        if (events[i].depth != depth + 1) continue;
        if (events[i].type == SUB_EVENT_KEYVAL) {
            json_member *new_members = (json_member *)realloc(members, (member_count + 1) * sizeof(json_member));
            if (!new_members) break;
            members = new_members;
            members[member_count++] = (json_member){ events[i].key, events[i].type, i, i };
        } else if (events[i].type == SUB_EVENT_SECTION_START) {
            size_t end = find_section_end(events, end_idx, i);
            json_member *new_members = (json_member *)realloc(members, (member_count + 1) * sizeof(json_member));
            if (!new_members) break;
            members = new_members;
            members[member_count++] = (json_member){ events[i].key, events[i].type, i, end };
            i = end;
        }
    }

    for (size_t i = 0; i < member_count; i++) {
        int found = 0;
        for (size_t k = 0; k < uniq_count; k++) {
            if (key_eq(uniq_keys[k], members[i].key)) {
                uniq_counts[k]++;
                found = 1;
                break;
            }
        }
        if (!found) {
            byte_span *nk = (byte_span *)realloc(uniq_keys, (uniq_count + 1) * sizeof(byte_span));
            size_t *nc = (size_t *)realloc(uniq_counts, (uniq_count + 1) * sizeof(size_t));
            if (!nk || !nc) {
                free(nk);
                free(nc);
                break;
            }
            uniq_keys = nk;
            uniq_counts = nc;
            uniq_keys[uniq_count] = members[i].key;
            uniq_counts[uniq_count] = 1;
            uniq_count++;
        }
    }

    int *emitted = (int *)calloc(uniq_count, sizeof(int));

    fputc('{', f);
    int first = 1;

    for (size_t i = 0; i < member_count; i++) {
        size_t key_idx = 0;
        for (; key_idx < uniq_count; key_idx++) {
            if (key_eq(uniq_keys[key_idx], members[i].key)) break;
        }
        if (key_idx >= uniq_count) continue;
        if (uniq_counts[key_idx] > 1 && emitted[key_idx]) continue;

        if (!first) fputc(',', f);
        first = 0;

        write_json_string(f, members[i].key);
        fputc(':', f);

        if (uniq_counts[key_idx] > 1) {
            fputc('[', f);
            int first_arr = 1;
            for (size_t j = 0; j < member_count; j++) {
                if (!key_eq(members[j].key, members[i].key)) continue;
                if (!first_arr) fputc(',', f);
                first_arr = 0;
                if (members[j].type == SUB_EVENT_KEYVAL) {
                    write_json_string(f, events[members[j].idx].value);
                } else {
                    write_object_range(f, events, members[j].idx + 1, members[j].end_idx, depth + 1);
                }
            }
            fputc(']', f);
            emitted[key_idx] = 1;
        } else {
            if (members[i].type == SUB_EVENT_KEYVAL) {
                write_json_string(f, events[members[i].idx].value);
            } else {
                write_object_range(f, events, members[i].idx + 1, members[i].end_idx, depth + 1);
            }
        }
    }

    fputc('}', f);

    free(emitted);
    free(uniq_keys);
    free(uniq_counts);
    free(members);
}

static void write_csv_cell(FILE *f, byte_span s) {
    int needs_quotes = 0;
    for (size_t i = 0; i < s.len; i++) {
        unsigned char c = s.ptr[i];
        if (c == '"' || c == ',' || c == '\n' || c == '\r') {
            needs_quotes = 1;
            break;
        }
    }
    if (!needs_quotes) {
        write_span(f, s);
        return;
    }
    fputc('"', f);
    for (size_t i = 0; i < s.len; i++) {
        unsigned char c = s.ptr[i];
        if (c == '"') fputc('"', f);
        fputc((int)c, f);
    }
    fputc('"', f);
}

static void sanitize_filename(char *dst, size_t dst_cap, byte_span name, size_t index) {
    if (!dst || dst_cap == 0) return;
    size_t pos = 0;
    if (name.ptr && name.len > 0) {
        for (size_t i = 0; i < name.len && pos + 1 < dst_cap; i++) {
            unsigned char c = name.ptr[i];
            if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
                c == '"'  || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
            dst[pos++] = (char)c;
        }
        dst[pos] = '\0';
        if (pos > 0) return;
    }
    snprintf(dst, dst_cap, "doc_%zu.bin", index);
}

static int write_outputs(const char *out_dir, const sgml_parse_result *r, const standardized_submission_metadata *m) {
    if (make_dir(out_dir) != 0) {
        fprintf(stderr, "Failed to create output dir: %s\n", out_dir);
        return -1;
    }

    if (m && m->count > 0) {
        char sub_path[1024];
        snprintf(sub_path, sizeof(sub_path), "%s\\submission_metadata.json", out_dir);
        FILE *sub = fopen(sub_path, "wb");
        if (sub) {
            write_object_range(sub, m->events, 0, m->count, -1);
            fputc('\n', sub);
            fclose(sub);
        } else {
            fprintf(stderr, "Failed to open submission metadata file: %s\n", sub_path);
        }
    }

    char meta_path[1024];
    snprintf(meta_path, sizeof(meta_path), "%s\\document_metadata.csv", out_dir);
    FILE *meta = fopen(meta_path, "wb");
    if (!meta) {
        fprintf(stderr, "Failed to open metadata file: %s\n", meta_path);
        return -1;
    }

    fputs("TYPE,SEQUENCE,FILENAME,DESCRIPTION\n", meta);

    for (size_t i = 0; i < r->doc_count; i++) {
        const document *doc = &r->docs[i];
        write_csv_cell(meta, doc->meta.type);
        fputc(',', meta);
        write_csv_cell(meta, doc->meta.sequence);
        fputc(',', meta);
        write_csv_cell(meta, doc->meta.filename);
        fputc(',', meta);
        write_csv_cell(meta, doc->meta.description);
        fputc('\n', meta);

        char fname[512];
        sanitize_filename(fname, sizeof(fname), doc->meta.filename, i + 1);
        char out_path[1200];
        snprintf(out_path, sizeof(out_path), "%s\\%s", out_dir, fname);

        FILE *out = fopen(out_path, "wb");
        if (!out) {
            fprintf(stderr, "Failed to write document: %s\n", out_path);
            continue;
        }

        // Use decoded buffer if uuencoded, otherwise write raw content
        if (doc->is_uuencoded && doc->decoded && doc->decoded_len > 0) {
            fwrite(doc->decoded, 1, doc->decoded_len, out);
        } else if (!doc->is_uuencoded && doc->content_start && doc->content_len > 0) {
            fwrite(doc->content_start, 1, doc->content_len, out);
        }

        fclose(out);
    }

    fclose(meta);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.txt> <output_dir>\n", argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_dir = argv[2];

    size_t in_len = 0;
    double t0 = now_ms();
    uint8_t *buf = load_file(input_path, &in_len);
    double t1 = now_ms();
    if (!buf) {
        fprintf(stderr, "Failed to load input: %s\n", input_path);
        return 1;
    }

    sgml_parse_stats stats = {0};
    double t2 = now_ms();
    submission_metadata sub = parse_submission_metadata(buf, in_len);
    double t3 = now_ms();
    standardized_submission_metadata std = standardize_submission_metadata(&sub);
    double t4 = now_ms();
    sgml_parse_result r = parse_sgml(buf, in_len, &stats);
    double t5 = now_ms();

    int w = write_outputs(output_dir, &r, &std);

    free_sgml_parse_result(&r);
    free_standardized_submission_metadata(&std);
    free_submission_metadata(&sub);
    free(buf);

    fprintf(stderr, "Timing (ms):\n");
    fprintf(stderr, "  load:              %.3f\n", (t1 - t0));
    fprintf(stderr, "  parse_sub_metadata: %.3f\n", (t3 - t2));
    fprintf(stderr, "  standardize_meta:  %.3f\n", (t4 - t3));
    fprintf(stderr, "  parse_sgml:        %.3f\n", (t5 - t4));
    fprintf(stderr, "  parse_total:       %.3f\n", (t5 - t2));

    return w == 0 ? 0 : 1;
}
