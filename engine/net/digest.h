#pragma once

#include "base/types.h"

#include <stddef.h>

// Fingerprints of bytes, from the TLS library that is here for https anyway.
namespace nx
{
    // 16 bytes.
    void md5(const void* data, size_t size, u8* out);
    // 20 bytes.
    void sha1(const void* data, size_t size, u8* out);
    // 32 bytes.
    void sha256(const void* data, size_t size, u8* out);
}
