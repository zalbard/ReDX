#pragma once

#include "Renderer.h"
#include "HelperStructs.hpp"
#include "..\ThirdParty\d3dx12.h"

template <typename T>
D3D12::VertexBuffer D3D12::Renderer::createVertexBuffer(const uint count, const T* const elements) {
    assert(elements && count >= 3);
    VertexBuffer buffer;
    const uint size = count * sizeof(T);
    // Allocate the buffer on the default heap
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
    const auto bufferDesc     = CD3DX12_RESOURCE_DESC::Buffer(size);
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &bufferDesc, D3D12_RESOURCE_STATE_COMMON,
                                                 nullptr, IID_PPV_ARGS(&buffer.resource)),
               "Failed to allocate a vertex buffer.");
    // Transition the memory buffer state for graphics/compute command queue type class
    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer.resource.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    m_graphicsCommandList->ResourceBarrier(1, &barrier);
    // Copy the vertices to video memory using the upload buffer
    // Max. alignment requirement for vertex data is 4 bytes
    constexpr uint64 alignment = 4;
    uploadData<alignment>(buffer, size, elements);
    // Initialize the vertex buffer view
    buffer.view = D3D12_VERTEX_BUFFER_VIEW{
        /* BufferLocation */ buffer.resource->GetGPUVirtualAddress(),
        /* SizeInBytes */    size,
        /* StrideInBytes */  sizeof(T)
    };
    return buffer;
}

template <uint N>
void D3D12::Renderer::drawIndexed(const VertexBuffer (&vbos)[N],
                                  const IndexBuffer* const ibos, const uint iboCount,
                                  const DynBitSet& drawMask) {
    D3D12_VERTEX_BUFFER_VIEW vboViews[N];
    for (uint i = 0; i < N; ++i) {
        vboViews[i] = vbos[i].view;
    }
    // Record the commands into the command list
    m_graphicsCommandList->IASetVertexBuffers(0, N, vboViews);
    for (uint i = 0; i < iboCount; ++i) {
        if (drawMask.testBit(i)) {
            m_graphicsCommandList->IASetIndexBuffer(&ibos[i].view);
            m_graphicsCommandList->DrawIndexedInstanced(ibos[i].count(), 1, 0, 0, 0);
        }
    }
}
