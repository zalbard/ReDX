#include "Buffer.h"
#include "Utility.h"

Buffer::Buffer()
    : size{0}
    , capacity{0}
    , ptr{nullptr} {}

Buffer::Buffer(const char* fileWithPath) {
    FILE* file;
    // Open the file.
    if (fopen_s(&file, fileWithPath, "rb")) {
        printError("File not found: %s", fileWithPath);
        TERMINATE();
    }
    // Get the file size in bytes.
    fseek(file, 0, SEEK_END);
    size = capacity = ftell(file);
    rewind(file);
    // Read the file.
    ptr = std::make_unique<byte_t[]>(capacity);
    fread(ptr.get(), 1, size, file);
    // Close the file.
    fclose(file);
}

byte_t* Buffer::data() {
    return ptr.get();
}

const byte_t* Buffer::data() const {
    return ptr.get();
}
