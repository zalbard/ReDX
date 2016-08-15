#include <cassert>
#include "DynBitSet.h"

DynBitSet::DynBitSet()
    : m_bits{nullptr}
    , m_bitCount{0}
    , m_dwordCount{0} {}

DynBitSet::DynBitSet(const size_t size)
    : m_bits{std::make_unique<uint32_t[]>((size + 31) / 32)}
    , m_bitCount{static_cast<uint32_t>(size)}
    , m_dwordCount{static_cast<uint32_t>((size + 31) / 32)} {
    reset(0);
}

DynBitSet::DynBitSet(const DynBitSet& other)
    : m_bits{std::make_unique<uint32_t[]>(other.m_dwordCount)}
    , m_bitCount{other.m_bitCount}
    , m_dwordCount{other.m_dwordCount} {
    memcpy(m_bits.get(), other.m_bits.get(), other.m_dwordCount * sizeof(uint32_t));
}

DynBitSet& DynBitSet::operator=(const DynBitSet& other) {
    if (this != &other) {
        m_bitCount = other.m_bitCount;
        if (m_dwordCount >= other.m_dwordCount) {
            // Reuse the currently allocated buffer.
            // The size exposed to the user will be identical to 'other'.
        } else {
            // Allocate a bigger buffer.
            m_bits = std::make_unique<uint32_t[]>(other.m_dwordCount);
        }
        m_dwordCount = other.m_dwordCount;
        memcpy(m_bits.get(), other.m_bits.get(), other.m_dwordCount * sizeof(uint32_t));
    }
    return *this;
}

DynBitSet::DynBitSet(DynBitSet&& other) = default;

DynBitSet& DynBitSet::operator=(DynBitSet&& other) = default;

DynBitSet::~DynBitSet() = default;

void DynBitSet::reset(const bool value) {
    const byte_t val = value ? 0xFF : 0x0;
    memset(m_bits.get(), val, m_dwordCount * sizeof(uint32_t));
}

void DynBitSet::clearBit(const size_t index) {
    assert(index < m_bitCount);
    m_bits[index / 32] &= ~(1 << (index % 32));
}

void DynBitSet::setBit(const size_t index) {
    assert(index < m_bitCount);
    m_bits[index / 32] |= 1 << (index % 32);
}

void DynBitSet::toggleBit(const size_t index) {
    assert(index < m_bitCount);
    m_bits[index / 32] ^= 1 << (index % 32);
}

bool DynBitSet::testBit(const size_t index) const {
    assert(index < m_bitCount);
    return 0 != (m_bits[index / 32] & 1 << (index % 32));
}
