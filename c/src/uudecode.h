#ifndef UUDECODE_H
#define UUDECODE_H

#include <stddef.h>
#include <stdint.h>

// Decode a uuencoded buffer (no begin/end lines) into out.
// Returns number of bytes written.
size_t uudecode(const uint8_t *in, size_t in_len, uint8_t *out);

#endif