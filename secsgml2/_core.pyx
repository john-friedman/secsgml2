# cython: language_level=3

from cpython.bytes cimport PyBytes_AsStringAndSize, PyBytes_FromStringAndSize
from cpython.unicode cimport PyUnicode_DecodeUTF8

cdef extern from "stdint.h":
    ctypedef unsigned char uint8_t

cdef extern from "stddef.h":
    ctypedef size_t size_t

cdef extern from "secsgml.h":
    ctypedef struct byte_span:
        const uint8_t *ptr
        size_t len

    ctypedef struct document_meta:
        byte_span type
        byte_span sequence
        byte_span filename
        byte_span description

    ctypedef struct document:
        document_meta meta
        const uint8_t *content_start
        size_t content_len
        uint8_t *decoded
        size_t decoded_len
        int is_uuencoded

    ctypedef struct sgml_parse_result:
        document *docs
        size_t doc_count
        size_t doc_cap
        sgml_status status

    ctypedef struct sgml_parse_stats:
        size_t doc_count
        size_t uuencoded_count

    cdef enum submission_event_type:
        SUB_EVENT_SECTION_START = 1
        SUB_EVENT_SECTION_END = 2
        SUB_EVENT_KEYVAL = 3

    ctypedef struct submission_event:
        submission_event_type type
        byte_span key
        byte_span value
        int depth

    ctypedef struct submission_metadata:
        submission_event *events
        size_t count
        size_t cap
        sgml_status status

    cdef enum sgml_status:
        SGML_STATUS_OK = 0
        SGML_STATUS_OOM = 1
        SGML_STATUS_TRUNCATED = 2

    sgml_parse_result parse_sgml(const uint8_t *buf, size_t len, sgml_parse_stats *stats)
    void free_sgml_parse_result(sgml_parse_result *r)
    submission_metadata parse_submission_metadata(const uint8_t *buf, size_t len)
    void free_submission_metadata(submission_metadata *m)

cdef extern from "standardize_submission_metadata.h":
    ctypedef struct standardized_submission_metadata:
        submission_event *events
        size_t count
        size_t cap
        uint8_t *arena
        size_t arena_len
        size_t arena_cap
        sgml_status status

    standardized_submission_metadata standardize_submission_metadata(const submission_metadata *m)
    void free_standardized_submission_metadata(standardized_submission_metadata *m)


cdef inline object _span_to_bytes(byte_span s):
    if s.ptr == NULL or s.len == 0:
        return b""
    return PyBytes_FromStringAndSize(<const char *>s.ptr, <Py_ssize_t>s.len)


cdef inline object _span_to_str(byte_span s):
    if s.ptr == NULL or s.len == 0:
        return ""
    return PyUnicode_DecodeUTF8(<const char *>s.ptr, <Py_ssize_t>s.len, "replace")


cdef size_t _find_section_end(submission_event *events, size_t count, size_t start_idx):
    cdef int depth = events[start_idx].depth
    cdef size_t i
    for i in range(start_idx + 1, count):
        if events[i].type == SUB_EVENT_SECTION_END and events[i].depth == depth:
            return i
    return count


cdef object _build_object(submission_event *events, size_t start_idx, size_t end_idx, int depth):
    cdef size_t i = start_idx
    cdef object result = {}
    cdef object key
    cdef object value
    cdef object existing
    cdef size_t end

    while i < end_idx:
        if events[i].depth != depth + 1:
            i += 1
            continue

        if events[i].type == SUB_EVENT_KEYVAL:
            key = _span_to_str(events[i].key)
            value = _span_to_str(events[i].value)
            existing = result.get(key, None)
            if existing is None:
                result[key] = value
            elif isinstance(existing, list):
                existing.append(value)
            else:
                result[key] = [existing, value]
            i += 1
            continue

        if events[i].type == SUB_EVENT_SECTION_START:
            end = _find_section_end(events, end_idx, i)
            key = _span_to_str(events[i].key)
            value = _build_object(events, i + 1, end, depth + 1)
            existing = result.get(key, None)
            if existing is None:
                result[key] = value
            elif isinstance(existing, list):
                existing.append(value)
            else:
                result[key] = [existing, value]
            i = end + 1
            continue

        i += 1

    return result


def parse_sgml_content_into_memory(bytes data, filter_document_types=None):
    cdef const char *cbuf
    cdef Py_ssize_t n
    cdef const uint8_t *buf
    cdef size_t length
    cdef submission_metadata sub
    cdef standardized_submission_metadata std
    cdef sgml_parse_stats stats
    cdef sgml_parse_result r
    cdef size_t i
    cdef document *doc
    cdef byte_span content_span
    cdef object content_bytes

    if PyBytes_AsStringAndSize(data, <char **>&cbuf, &n) < 0:
        raise ValueError("Expected bytes input")

    buf = <const uint8_t *>cbuf
    length = <size_t>n

    sub = parse_submission_metadata(buf, length)
    std = standardize_submission_metadata(&sub)
    stats.doc_count = 0
    stats.uuencoded_count = 0
    r = parse_sgml(buf, length, &stats)

    try:
        if sub.status != SGML_STATUS_OK:
            if sub.status == SGML_STATUS_OOM:
                raise MemoryError("parse_submission_metadata: out of memory")
            raise RuntimeError("parse_submission_metadata: truncated or failed")

        if std.status != SGML_STATUS_OK:
            if std.status == SGML_STATUS_OOM:
                raise MemoryError("standardize_submission_metadata: out of memory")
            raise RuntimeError("standardize_submission_metadata: truncated or failed")

        if r.status != SGML_STATUS_OK:
            if r.status == SGML_STATUS_OOM:
                raise MemoryError("parse_sgml: out of memory")
            raise RuntimeError("parse_sgml: truncated or failed")

        metadata = _build_object(std.events, 0, std.count, -1)

        documents_meta = []
        documents = []

        for i in range(r.doc_count):
            doc = &r.docs[i]

            doc_meta = {
                "type": _span_to_str(doc.meta.type),
                "sequence": _span_to_str(doc.meta.sequence),
                "filename": _span_to_str(doc.meta.filename),
                "description": _span_to_str(doc.meta.description),
            }

            documents_meta.append(doc_meta)

            if doc.is_uuencoded and doc.decoded != NULL and doc.decoded_len > 0:
                content_bytes = PyBytes_FromStringAndSize(
                    <const char *>doc.decoded, <Py_ssize_t>doc.decoded_len
                )
            else:
                content_span.ptr = doc.content_start
                content_span.len = doc.content_len
                content_bytes = _span_to_bytes(content_span)

            documents.append(content_bytes)

        metadata["documents"] = documents_meta

        if filter_document_types:
            doc_metas = metadata["documents"]
            indices = [i for i, m in enumerate(doc_metas) if m["type"] in filter_document_types]
            metadata["documents"] = [doc_metas[i] for i in indices]
            documents = [documents[i] for i in indices]
        
        for file_num, content in enumerate(documents):
            metadata["documents"][file_num]["secsgml_size_bytes"] = len(content)

        return metadata, documents
    finally:
        free_sgml_parse_result(&r)
        free_standardized_submission_metadata(&std)
        free_submission_metadata(&sub)
