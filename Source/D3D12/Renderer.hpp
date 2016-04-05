#pragma once

#include <d3dx12.h>
#include "Renderer.h"
#include "HelperStructs.hpp"

namespace D3D12 {
    template <typename T>
    inline auto Renderer::createVertexBuffer(const uint count, const T* elements)
    -> VertexBuffer {
        assert(elements && count >= 3);
        VertexBuffer buffer;
        const uint size = count * sizeof(T);
        // Allocate the buffer on the default heap.
        const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
        const auto resourceDesc   = CD3DX12_RESOURCE_DESC::Buffer(size);
        CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                     &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                                     nullptr, IID_PPV_ARGS(&buffer.resource)),
                   "Failed to allocate a vertex buffer.");
        // Transition the buffer state for the graphics/compute command queue type class.
        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer.resource.Get(),
                                                   D3D12_RESOURCE_STATE_COMMON,
                                                   D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        m_graphicsContext.commandList(0)->ResourceBarrier(1, &barrier);
        // Max. alignment requirement for vertex data is 4 bytes.
        constexpr uint64 alignment = 4;
        // Copy vertices into the upload buffer.
        const uint offset = copyToUploadBuffer<alignment>(size, elements);
        // Copy the data from the upload buffer into the video memory buffer.
        m_copyContext.commandList(0)->CopyBufferRegion(buffer.resource.Get(), 0,
                                                       m_uploadBuffer.resource.Get(), offset,
                                                       size);
        // Initialize the vertex buffer view.
        buffer.view.BufferLocation = buffer.resource->GetGPUVirtualAddress();
        buffer.view.SizeInBytes    = size;
        buffer.view.StrideInBytes  = sizeof(T);
        return buffer;
    }

    template <uint N>
    inline void Renderer::drawIndexed(const VertexBuffer (&vbos)[N],
                                      const IndexBuffer* ibos, const uint iboCount,
                                      const uint16* materialIndices,
                                      const DynBitSet& drawMask) {
        D3D12_VERTEX_BUFFER_VIEW vboViews[N];
        for (uint i = 0; i < N; ++i) {
            vboViews[i] = vbos[i].view;
        }
        // Record commands into the command list.
        const auto graphicsCommandList = m_graphicsContext.commandList(0);
        graphicsCommandList->IASetVertexBuffers(0, N, vboViews);
        uint16 matId = UINT16_MAX;
        for (uint i = 0; i < iboCount; ++i) {
            if (drawMask.testBit(i)) {
                if (matId != materialIndices[i]) {
                    matId  = materialIndices[i];
                    // Set the object's material index.
                    graphicsCommandList->SetGraphicsRoot32BitConstant(0, matId, 0);
                }
                // Draw the object.
                graphicsCommandList->IASetIndexBuffer(&ibos[i].view);
                graphicsCommandList->DrawIndexedInstanced(ibos[i].count(), 1, 0, 0, 0);
            }
        }
    }

    template <uint64 alignment>
    inline auto Renderer::copyToUploadBuffer(const uint size, const void* data)
    -> uint {
        assert(data);
        uint offset;
        // Compute the address within the upload buffer which we will copy the data to.
        byte* address;
        std::tie(address, offset) = reserveChunkOfUploadBuffer<alignment>(size);
        // Load the data into the upload buffer.
        memcpy(address, data, size);
        // Return the offset to the beginning of the data.
        return offset;
    }

    template <uint64 alignment>
    inline auto Renderer::reserveChunkOfUploadBuffer(const uint size)
    -> std::pair<byte*, uint> {
        assert(size > 0);
        // Compute the address within the upload buffer which we will copy the data to.
        byte*  alignedAddress = align<alignment>(m_uploadBuffer.begin + m_uploadBuffer.offset);
        uint64 alignedOffset  = alignedAddress - m_uploadBuffer.begin;
        // Check whether the upload buffer has sufficient space left.
        const int64 remainingCapacity = m_uploadBuffer.capacity - alignedOffset;
        const bool wrapAround = remainingCapacity < size;
        if (wrapAround) {
            // Recompute 'alignedAddress' and 'alignedOffset'.
            alignedAddress = align<alignment>(m_uploadBuffer.begin);
            alignedOffset  = alignedAddress - m_uploadBuffer.begin;
            // Make sure the upload buffer is sufficiently large.
            #ifdef _DEBUG
            {
                const int64 alignedCapacity = m_uploadBuffer.capacity - alignedOffset;
                if (alignedCapacity < size) {
                    printError("Insufficient upload buffer capacity: "
                               "current (aligned): %i, required: %u.",
                               alignedCapacity, size);
                    TERMINATE();
                }
            }
            #endif
        }
        const uint alignedEnd = static_cast<uint>(alignedOffset + size);
        // 1. Make sure we do not overwrite the current segment of the upload buffer.
        // (currSegStart == offset) is a perfectly valid configuration;
        // in order to maintain this invariant, we should execute all copies
        // (clear the buffer) in cases where (currSegStart == alignedEnd).
        // Case A: |====OFFS~~~~CURR~~~~AEND====|
        const bool caseA = (m_uploadBuffer.offset < m_uploadBuffer.currSegStart) &
                           (m_uploadBuffer.currSegStart <= alignedEnd);
        // Case B: |~~~~CURR~~~~OFFS~~~~AEND----| + wrap-around
        //     or: |~~~~CURR~~~~AEND====OFFS----| + wrap-around
        const bool caseB = (m_uploadBuffer.currSegStart <= alignedEnd) & wrapAround;
        // Case C: |~~~~OFFS~~~~AEND----CURR====| + wrap-around
        //     or: |~~~~OFFS~~~~CURR~~~~AEND====| + wrap-around
        //     or: |~~~~AEND====OFFS----CURR====| + wrap-around
        const bool caseC = (m_uploadBuffer.offset < m_uploadBuffer.currSegStart) & wrapAround;
        // If there is not enough space for the new data, we have to execute
        // all copies first, which will allow us to safely overwrite the old data.
        const bool executeAllCopies = caseA | caseB | caseC;
        // 2. Make sure we do not overwrite the previous segment of the upload buffer.
        // (prevSegStart == offset) means we are about to overwrite the previous segment.
        // Case D: |====OFFS~~~~PREV~~~~AEND====|
        const bool caseD = (m_uploadBuffer.offset <= m_uploadBuffer.prevSegStart) &
                           (m_uploadBuffer.prevSegStart < alignedEnd);
        // Case E: |~~~~PREV~~~~AEND====OFFS----| + wrap-around
        //     or: |~~~~PREV~~~~OFFS~~~~AEND----| + wrap-around
        //     or: |~~~~OFFS~~~~PREV~~~~AEND----| + wrap-around
        const bool caseE = (m_uploadBuffer.prevSegStart < alignedEnd) & wrapAround;
        // We may have to wait until the data within the previous segment can be safely overwritten.
        const bool waitForPrevCopies = caseD | caseE;
        // Move the offset to the beginning of the data.
        m_uploadBuffer.offset = static_cast<uint>(alignedOffset);
        // Check whether any copies to the GPU have to be performed.
        if (executeAllCopies || waitForPrevCopies) {
            executeCopyCommands(executeAllCopies);
        }
        // Move the offset to the end of the data.
        m_uploadBuffer.offset = alignedEnd;
        // Return the address of and the offset to the beginning of the data.
        return {alignedAddress, static_cast<uint>(alignedOffset)};
    }
} // namespace D3D12
