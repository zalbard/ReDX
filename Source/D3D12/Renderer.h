#pragma once

#include <dxgi1_4.h>
#include "HelperStructs.h"
#include "..\Common\Constants.h"

namespace D3D12 {
    class Renderer {
    public:
        Renderer();
        RULE_OF_ZERO_MOVE_ONLY(Renderer);
        // Creates a vertex buffer for the vertex array with the specified number of vertices
        VertexBuffer createVertexBuffer(const uint count, const Vertex* const vertices);
        // Creates an index buffer for the index array with the specified number of indices
        IndexBuffer createIndexBuffer(const uint count, const uint* const indices);
        // Executes all pending copy commands, blocking the thread until they finish
        void executeCopyCommands();
        // Initializes the frame rendering process
        void startFrame();
        // Draws the geometry from the indexed vertex buffer to the frame buffer
        void draw(const VertexBuffer& vbo, const IndexBuffer& ibo);
        // Finalizes the frame rendering process
        void finalizeFrame();
        // Terminates the rendering process
        void stop();
    private:
        // Configures the rendering pipeline, including the shaders
        void configurePipeline();
        // Uploads the data of the specified size in bytes and alignment
        // to the memory buffer via an intermediate upload buffer
        // Expects 'dst' in D3D12_RESOURCE_STATE_COPY_DEST, and transitions it to the 'finalState'
        template<uint64 alignment>
        void uploadData(MemoryBuffer& dst, const uint size, const void* const data,
                        const D3D12_RESOURCE_STATES finalState);
    private:
        /* Rendering parameters */
        D3D12_VIEWPORT                             m_viewport;
        D3D12_RECT                                 m_scissorRect;
        uint                                       m_backBufferIndex;
        /* Direct3D resources */
        ComPtr<ID3D12DeviceEx>                     m_device;
        CommandQueue<QueueType::COPY, 1>           m_copyCommandQueue;
        CommandQueue<QueueType::GRAPHICS, BUF_CNT> m_graphicsCommandQueue;
        ComPtr<IDXGISwapChain3>                    m_swapChain;
        DescriptorPool<DescType::RTV>              m_rtvPool;
        ComPtr<ID3D12Resource>                     m_renderTargets[BUF_CNT];
        DescriptorPool<DescType::DSV>              m_dsvPool;
        ComPtr<ID3D12Resource>                     m_depthBuffer;
        UploadBuffer                               m_uploadBuffer;
        /* Pipeline objects */
        ComPtr<ID3D12GraphicsCommandList>          m_copyCommandList;
        ComPtr<ID3D12RootSignature>                m_graphicsRootSignature;
        ComPtr<ID3D12PipelineState>                m_graphicsPipelineState;
        ComPtr<ID3D12GraphicsCommandList>          m_graphicsCommandList;
    };
} // namespace D3D12
