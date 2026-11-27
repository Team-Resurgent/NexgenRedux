#include "net/digest.h"

#include <mbedtls/md5.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>

namespace nx
{
    void md5(const void* data, size_t size, u8* out)
    {
        mbedtls_md5(static_cast<const unsigned char*>(data), size, out);
    }

    void sha1(const void* data, size_t size, u8* out)
    {
        mbedtls_sha1(static_cast<const unsigned char*>(data), size, out);
    }

    void sha256(const void* data, size_t size, u8* out)
    {
        mbedtls_sha256(static_cast<const unsigned char*>(data), size, out, 0);
    }
}
