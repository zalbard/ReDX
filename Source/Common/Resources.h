#pragma once

#include <memory>
#include "Definitions.h"

// A linear allocator which uses N static, non-growing memory regions of the same size.
// It works as a ring buffer: switching from the buffer (N - 1) causes the buffer 0 to be used.
template <size_t N>
class BufferedLinearAllocator {
public:
    RULE_OF_ZERO_MOVE_ONLY(BufferedLinearAllocator);
    // Ctor which allocates 'size' bytes per buffer.
    BufferedLinearAllocator(const size_t size);
    // Allocates 'size' bytes according to the specified alignment restriction.
    template <size_t alignment>
    void* allocate(const size_t size);
    // Makes subsequent allocations return pointers to locations within the next buffer.
    void switchToNextBuffer();
private:
    // Returns the offset to the end of the current buffer.
    // It is the same as the beginning of the next buffer if wrap around doesn't occur.
    size_t computeBufferEndOffset() const;
private:
    size_t                    m_size;       // Size of each buffer
    size_t                    m_offset;     // Offset from the beginning of the heap region
    std::unique_ptr<byte_t[]> m_heapRegion; // Heap region backing N buffers
};
