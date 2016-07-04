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
    // Sets the internal pointer to the beginning of the next buffer.
    // Makes subsequent allocations occur within the next buffer.
    void switchToNextBuffer();
    // Resets the allocator to its initial state.
    void reset();
private:
    // Returns the pointer to the end of the current buffer.
    // It coincides with the beginning of the next buffer if wrap around does not occur.
    const byte_t* computeBufferEnd() const;
private:
    size_t                    m_size;       // Size of each buffer
    byte_t*                   m_current;    // Current (free) position within the heap region
    std::unique_ptr<byte_t[]> m_heapRegion; // Heap region backing N buffers
};

using LinearAllocator = BufferedLinearAllocator<1>;
