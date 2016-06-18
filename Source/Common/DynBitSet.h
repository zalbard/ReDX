#pragma once

#include <memory>
#include "Definitions.h"

// BitSet with size specified at runtime.
class DynBitSet {
public:
    RULE_OF_FIVE(DynBitSet);
    // Ctor; performs zero-initialization.
    DynBitSet();
    // Ctor; takes the size (number of bits) as input.
    explicit DynBitSet(const size_t size);
    // Resets the values of all bits to 'value' (0 or 1).
    void reset(const bool value);
    // Sets the value of the specified bit to 0.
    void clearBit(const size_t index);
    // Sets the value of the specified bit to 1.
    void setBit(const size_t index);
    // Inverts the value of the specified bit.
    void toggleBit(const size_t index);
    // Returns 'true' if the specified bit is 1, 'false' otherwise.
    bool testBit(const size_t index) const;
private:
    std::unique_ptr<uint32_t[]> m_bits;
    uint32_t                    m_bitCount;
    uint32_t                    m_dwordCount;
};
