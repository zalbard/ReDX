#pragma once

#include "Math.h"
#include "Resources.h"

template <size_t N>
inline BufferedLinearAllocator<N>::BufferedLinearAllocator(const size_t size)
    : m_size{size}
    , m_offset{0}
    , m_heapRegion{std::make_unique<byte_t[]>(size * N)} {
    static_assert(N >= 1, "BufferedLinearAllocator must have at least 1 buffer.");
    assert(size >= 64 && "The size of the buffer cannot be smaller than 64 bytes.");
}

template<size_t N>
template<size_t alignment>
inline auto BufferedLinearAllocator<N>::allocate(const size_t size)
-> void* {
    const size_t alignedOffset = align<alignment>(m_offset);
    const size_t alignedEnd    = alignedOffset + size;
    // computeBufferEndOffset() assumes that 'm_offset' doesn't coincide with the end of the buffer.
    assert(alignedEnd < computeBufferEndOffset() &&
           "This allocation would cause buffer overflow.");
    m_offset = alignedEnd;
    return m_heapRegion.get() + alignedOffset;
}

template<size_t N>
inline auto BufferedLinearAllocator<N>::computeBufferEndOffset() const
-> size_t {
    // Compute the offset with respect to the beginning of the current buffer.
    const size_t bufferOffset = m_offset % m_size;
    // Compute the offset to the beginning of the current buffer.
    const size_t bufferBegin  = m_offset - bufferOffset;
    // Adjust it by the size of the buffer.
    return bufferBegin + m_size;
}

template <size_t N>
inline void BufferedLinearAllocator<N>::switchToNextBuffer() {
    size_t offset = computeBufferEndOffset();
    // Wrap around if necessary.
    offset = offset % (m_size * N);
    // Store the result.
    m_offset = offset;
}
