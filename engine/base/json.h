#pragma once

#include <stddef.h>

namespace nx
{
    // Reads one value from the top level of a JSON object. Nested objects and arrays are skipped over.
    // False if the key isn't there, has another type, or the text isn't valid up to that point.
    bool jsonString(const char* text, const char* key, char* out, size_t size);
    // Calls back with every string at the top level. Stops at anything malformed.
    using JsonString = void (*)(const char* key, const char* value, void* user);
    void jsonEachString(const char* text, char* buffer, size_t size, JsonString callback, void* user);
    bool jsonNumber(const char* text, const char* key, double& out);
}
