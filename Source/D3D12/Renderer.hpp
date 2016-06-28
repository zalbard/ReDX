#pragma once

#include <d3dx12.h>
#include "HelperStructs.hpp"
#include "Renderer.h"

namespace D3D12 {
    template <typename T>
    inline auto Renderer::createVertexBuffer(const size_t count, const T* elements)
    -> VertexBuffer {
        assert(elements && count >= 3);
        const size_t size = count * sizeof(T);
        VertexBuffer buffer;
        // Allocate the buffer on the default heap.
        const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
        const auto resourceDesc   = CD3DX12_RESOURCE_DESC::Buffer(size);
        CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                     &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                                     nullptr, IID_PPV_ARGS(&buffer.resource)),
                   "Failed to allocate a vertex buffer.");
        // Transition the state of the buffer for the graphics/compute command queue type class.
        const D3D12_TRANSITION_BARRIER barrier{buffer.resource.Get(),
                                               D3D12_RESOURCE_STATE_COMMON,
                                               D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER};
        m_graphicsContext.commandList(0)->ResourceBarrier(1, &barrier);
        // Linear subresource copying must be aligned to 512 bytes.
        constexpr size_t alignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
        const     size_t offset    = copyToUploadBuffer<alignment>(size, elements);
        // Copy the data from the upload buffer into the video memory buffer.
        m_copyContext.commandList(0)->CopyBufferRegion(buffer.resource.Get(), 0,
                                                       m_uploadBuffer.resource.Get(), offset,
                                                       size);
        // Initialize the vertex buffer view.
        buffer.view.BufferLocation = buffer.resource->GetGPUVirtualAddress();
        buffer.view.SizeInBytes    = static_cast<uint32_t>(size);
        buffer.view.StrideInBytes  = sizeof(T);
        return buffer;
    }

    template <size_t alignment>
    inline auto Renderer::copyToUploadBuffer(const size_t size, const void* data)
    -> size_t {
        assert(data);
        // Compute the address within the upload buffer which we will copy the data to.
        byte_t* address;
        size_t  offset;
        std::tie(address, offset) = reserveChunkOfUploadBuffer<alignment>(size);
        // Load the data into the upload buffer.
        memcpy(address, data, size);
        // Return the offset to the beginning of the data.
        return offset;
    }

    template <size_t alignment>
    inline auto Renderer::reserveChunkOfUploadBuffer(const size_t size)
    -> std::pair<byte_t*, size_t> {
        assert(size > 0);
        const sign_t signedSize = static_cast<sign_t>(size);
        // Compute the address within the upload buffer which we will copy the data to.
        byte_t* address = align<alignment>(m_uploadBuffer.begin + m_uploadBuffer.offset);
        size_t  offset  = address - m_uploadBuffer.begin;
        size_t  shift   = offset  - m_uploadBuffer.offset;
        // Compute the remaining capacity of the upload buffer.
        sign_t  remain  = m_uploadBuffer.remainingCapacity() - shift;
        // Check if there is sufficient space left between the offset and the end of the buffer.
        const sign_t distToEnd = m_uploadBuffer.capacity - offset;
        if (distToEnd < signedSize) {
            // Wrap around.
            address = align<alignment>(m_uploadBuffer.begin);
            offset  = address - m_uploadBuffer.begin;
            shift   = offset;
            remain -= distToEnd + shift;
            #ifndef NDEBUG
            {
                // Make sure the upload buffer is sufficiently large.
                const sign_t alignedCapacity = m_uploadBuffer.capacity - offset;
                if (alignedCapacity < signedSize) {
                    printError("Insufficient upload buffer capacity: "
                               "current (aligned): %td, required: %zu.",
                               alignedCapacity, size);
                    TERMINATE();
                }
            }
            #endif
        }
        // We have found a suitable contiguous segment of the upload buffer.
        // Determine whether there is any currently stored data we have to upload first.
        if (remain <= signedSize) {
            const size_t prevSegSize = m_uploadBuffer.previousSegmentSize();
            // If the remaining capacity is insufficient to hold both
            // the previous segment and the new data, we have to upload
            // the data from both segments before proceeding further.
            const sign_t totalSize = prevSegSize + size;
            const bool executeAllCopies = (remain <= totalSize);
            // Move the offset to the beginning of the data.
            // It will become the beginning of the new segment.
            m_uploadBuffer.offset = static_cast<uint32_t>(offset);
            executeCopyCommands(executeAllCopies);
        }
        // Move the offset to the end of the data.
        m_uploadBuffer.offset = static_cast<uint32_t>(offset + size);
        // Return the address of and the offset to the beginning of the data.
        return {address, offset};
    }
} // namespace D3D12
