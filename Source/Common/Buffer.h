#pragma once

#include <memory>
#include "Definitions.h"

struct Buffer {
    Buffer();
    // Initializes the buffer by reading the file.
    explicit Buffer(const char* fileWithPath);
    /* Accessors */
    byte_t*       data();
    const byte_t* data() const;
public:
    std::unique_ptr<byte_t[]> ptr;          // Storage array
    uint32_t                  size;         // Bytes currently used
    uint32_t                  capacity;     // Bytes available in total
};
