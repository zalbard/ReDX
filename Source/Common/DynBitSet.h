#pragma once

#include <memory>
#include "Definitions.h"

// BitSet with size specified at runtime
class DynBitSet {
public:
    RULE_OF_FIVE(DynBitSet);
    // Ctor; performs zero-initialization
    DynBitSet();
    // Ctor; takes the size (number of bits) as input
    explicit DynBitSet(const uint size);
    // Resets the values of all bits to 'value' (0 or 1)
    void reset(const bool value);
    // Sets the value of the specified bit to 0
    void clearBit(const uint index);
    // Sets the value of the specified bit to 1
    void setBit(const uint index);
    // Inverts the value of the specified bit
    void toggleBit(const uint index);
    // Returns 'true' if the specified bit is 1, 'false' otherwise
    bool testBit(const uint index) const;
private:
    std::unique_ptr<uint[]> m_bits;
    uint                    m_bitCount;
    uint                    m_dwordCount;
};
