#pragma once

#include "Math.h"
#include "Resources.h"

template <size_t N>
inline BufferedLinearAllocator<N>::BufferedLinearAllocator(const size_t size)
    : m_size{size}
    , m_heapRegion{std::make_unique<byte_t[]>(size * N)} {
    static_assert(N >= 1, "BufferedLinearAllocator must have at least 1 buffer.");
    assert(size >= 64 && "The size of the buffer cannot be smaller than 64 bytes.");
    m_current = m_heapRegion.get();
}

template<size_t N>
template<size_t alignment>
inline auto BufferedLinearAllocator<N>::allocate(const size_t size)
-> void* {
    byte_t* alignedPtr = align<alignment>(m_current);
    byte_t* alignedEnd = alignedPtr + size;
    // computeBufferEnd() assumes that 'm_current' does not coincide with the end of the buffer.
    assert(alignedEnd < computeBufferEnd() &&
           "This allocation would cause buffer overflow.");
    m_current = alignedEnd;
    return alignedPtr;
}

template<size_t N>
inline auto BufferedLinearAllocator<N>::computeBufferEnd() const
-> const byte_t* {
    // Compute the offset from the beginning of the heap region corresponding to 'm_current'.
    const size_t offset = m_current - m_heapRegion.get();
    // Compute the offset with respect to the beginning of the current buffer.
    const size_t bufferOffset = offset % m_size;
    // Compute the pointer to the beginning of the current buffer.
    const byte_t* bufferBegin = m_current - bufferOffset;
    // Offset the pointer by the size of the buffer.
    return bufferBegin + m_size;
}

template <size_t N>
inline void BufferedLinearAllocator<N>::switchToNextBuffer() {
    byte_t*       heapBegin = m_heapRegion.get();
    const byte_t* bufferEnd = computeBufferEnd();
    // Transform it into an offset.
    size_t offset = bufferEnd - heapBegin;
    // Wrap around if necessary.
    offset = offset % (m_size * N);
    // Store the result.
    m_current = heapBegin + offset;
}
