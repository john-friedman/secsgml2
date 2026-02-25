# cython: language_level=3

import warnings

from cpython.bytes cimport (
    PyBytes_AsStringAndSize,
    PyBytes_AsString,
    PyBytes_FromStringAndSize,
    _PyBytes_Resize,
)
from cpython.ref cimport PyObject

cdef extern from "stdint.h":
    ctypedef unsigned char uint8_t

cdef extern from "stddef.h":
    ctypedef size_t size_t

cdef extern from "uudecode.h":
    size_t uudecode(const uint8_t *inp, size_t in_len, uint8_t *out)


cdef bytes _decode_uu_body(bytes data):
    cdef const char *cbuf
    cdef Py_ssize_t n
    cdef const uint8_t *buf
    cdef size_t length
    cdef size_t out_len
    cdef object out
    cdef PyObject *out_obj
    cdef char *out_buf

    if PyBytes_AsStringAndSize(data, <char **>&cbuf, &n) < 0:
        raise ValueError("Expected bytes input")

    buf = <const uint8_t *>cbuf
    length = <size_t>n

    # Decoded size is <= encoded size (includes length bytes + newlines)
    out = PyBytes_FromStringAndSize(NULL, n)
    if out is None:
        raise MemoryError("Unable to allocate output buffer")

    out_buf = PyBytes_AsString(out)
    out_len = uudecode(buf, length, <uint8_t *>out_buf)

    out_obj = <PyObject *>out
    if _PyBytes_Resize(&out_obj, <Py_ssize_t>out_len) < 0:
        raise MemoryError("Unable to resize output buffer")
    out = <object>out_obj

    return out


def _find_uu_body_bounds(bytes data):
    cdef Py_ssize_t n = len(data)
    cdef Py_ssize_t i = 0
    cdef Py_ssize_t line_start = 0
    cdef Py_ssize_t line_end = 0
    cdef int lines_checked = 0
    cdef Py_ssize_t begin_body_start = -1
    cdef Py_ssize_t begin_line_end = -1
    cdef Py_ssize_t end_line_start = -1

    while i < n:
        line_start = i
        while i < n and data[i] not in (10, 13):  # \n or \r
            i += 1
        line_end = i

        if lines_checked < 3:
            if line_end - line_start >= 9 and data[line_start:line_start + 9] == b"begin 644":
                if line_end == line_start + 9 or data[line_start + 9] in b" \t":
                    begin_line_end = line_end
                    # skip line break(s)
                    if i < n and data[i] == 13:
                        i += 1
                    if i < n and data[i] == 10:
                        i += 1
                    begin_body_start = i
                    break
            lines_checked += 1

        # skip line break(s)
        if i < n and data[i] == 13:
            i += 1
        if i < n and data[i] == 10:
            i += 1

    if begin_body_start < 0:
        return None

    # find end line after begin
    while i < n:
        line_start = i
        while i < n and data[i] not in (10, 13):
            i += 1
        line_end = i

        if line_end - line_start >= 3 and data[line_start:line_start + 3] == b"end":
            if line_end == line_start + 3 or data[line_start + 3] in b" \t":
                end_line_start = line_start
                break

        if i < n and data[i] == 13:
            i += 1
        if i < n and data[i] == 10:
            i += 1

    if end_line_start < 0:
        return None

    return (begin_body_start, end_line_start)


def is_uuencoded(bytes data):
    return _find_uu_body_bounds(data) is not None


def strip_uu_wrappers(bytes data):
    cdef object bounds = _find_uu_body_bounds(data)
    if bounds is None:
        return data
    cdef Py_ssize_t start = bounds[0]
    cdef Py_ssize_t end = bounds[1]
    return data[start:end]


def decode_uuencoded_content(bytes data, no_warnings=True):
    cdef object bounds = _find_uu_body_bounds(data)
    if bounds is None:
        if not no_warnings:
            warnings.warn(
                "Input does not appear uuencoded; returning original bytes",
                RuntimeWarning,
                stacklevel=2,
            )
        return data
    cdef Py_ssize_t start = bounds[0]
    cdef Py_ssize_t end = bounds[1]
    return _decode_uu_body(data[start:end])
