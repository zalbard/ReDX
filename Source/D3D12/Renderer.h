#pragma once

#include <dxgi1_4.h>
#include "HelperStructs.h"
#include "..\Common\Constants.h"

namespace D3D12 {
    class Renderer {
    public:
        Renderer();
        RULE_OF_ZERO_MOVE_ONLY(Renderer);
        // Creates a constant buffer for the data of the specified size in bytes
        ConstantBuffer createConstantBuffer(const uint size, const void* const data = nullptr);
        // Creates a vertex buffer for the vertex array with the specified number of vertices
        VertexBuffer createVertexBuffer(const uint count, const Vertex* const vertices);
        // Creates an index buffer for the index array with the specified number of indices
        IndexBuffer createIndexBuffer(const uint count, const uint* const indices);
        // Sets the view-projection matrix in the shaders
        void setViewProjMatrix(const XMMATRIX& viewProjMat);
        // Executes all pending copy commands, blocking the thread until they finish
        // If 'clearUploadBuffer' is set to true, the function will reclaim
        // the used buffer space at the cost of blocking the thread
        void executeCopyCommands(const bool clearUploadBuffer);
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
        template<uint64 alignment>
        void uploadData(MemoryBuffer& dst, const uint size, const void* const data);
    private:
        /* Rendering parameters */
        D3D12_VIEWPORT                               m_viewport;
        D3D12_RECT                                   m_scissorRect;
        uint                                         m_backBufferIndex;
        /* Direct3D resources */
        ComPtr<ID3D12DeviceEx>                       m_device;
        CommandQueue<QueueType::COPY, 1>             m_copyCommandQueue;
        CommandQueue<QueueType::GRAPHICS, FRAME_CNT> m_graphicsCommandQueue;
        ComPtr<IDXGISwapChain3>                      m_swapChain;
        DescriptorPool<DescType::RTV>                m_rtvPool;
        ComPtr<ID3D12Resource>                       m_renderTargets[BUF_CNT];
        DescriptorPool<DescType::DSV>                m_dsvPool;
        ComPtr<ID3D12Resource>                       m_depthBuffer;
        /* Pipeline objects */
        UploadBuffer                                 m_uploadBuffer;
        ComPtr<ID3D12GraphicsCommandList>            m_copyCommandList;
        ConstantBuffer                               m_constantBuffer;
        ComPtr<ID3D12RootSignature>                  m_graphicsRootSignature;
        ComPtr<ID3D12PipelineState>                  m_graphicsPipelineState;
        ComPtr<ID3D12GraphicsCommandList>            m_graphicsCommandList;
    };
} // namespace D3D12
