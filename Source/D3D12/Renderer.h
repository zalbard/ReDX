#pragma once

#include <dxgi1_4.h>
#include <DirectXMathSSE4.h>
#include "HelperStructs.h"
#include "..\Common\Constants.h"
#include "..\Common\DynBitSet.h"

namespace D3D12 {
    class Renderer {
    public:
        RULE_OF_ZERO_MOVE_ONLY(Renderer);
        Renderer();
        // Creates a constant buffer for the data of the specified size in bytes
        ConstantBuffer createConstantBuffer(const uint size, const void* const data = nullptr);
        // Creates a vertex attribute buffer for the vertex array of 'count' elements
        template <typename T>
        VertexBuffer createVertexBuffer(const uint count, const T* const elements);
        // Creates an index buffer for the index array with the specified number of indices
        IndexBuffer createIndexBuffer(const uint count, const uint* const indices);
        // Sets the view-projection matrix in the shaders
        void setViewProjMatrix(DirectX::FXMMATRIX viewProjMat);
        // Submits all pending copy commands for execution, and begins a new segment
        // of the upload buffer. As a result, the previous segment of the buffer becomes
        // available for writing. 'immediateCopy' flag ensures that all copies from the
        // current segment are also completed during this call (at the cost of blocking
        // the thread), therefore making the entire buffer free and available for writing.
        void executeCopyCommands(const bool immediateCopy = false);
        // Initializes the frame rendering process
        void startFrame();
        template <uint N>
        // Draws geometry using 'N' vertex attribute buffers and 'iboCount' index buffers
        // 'drawMask' indicates whether geometry (within 'ibos') should be drawn
        void drawIndexed(const VertexBuffer (&vbos)[N],
                         const IndexBuffer* const ibos, const uint iboCount,
                         const DynBitSet& drawMask);
        // Finalizes the frame rendering process
        void finalizeFrame();
        // Terminates the rendering process
        void stop();
    private:
        // Configures the rendering pipeline, including the shaders
        void configurePipeline();
        // Uploads the data of the specified size in bytes and alignment
        // to the video memory buffer 'dst' via the intermediate upload buffer
        template<uint64 alignment>
        void uploadData(ID3D12Resource* const dst, const uint size, const void* const data);
    private:
        /* Rendering parameters */
        D3D12_VIEWPORT                               m_viewport;
        D3D12_RECT                                   m_scissorRect;
        uint                                         m_backBufferIndex;
        /* Direct3D resources */
        ComPtr<ID3D12DeviceEx>                       m_device;
        CommandQueue<QueueType::COPY, 2>             m_copyCommandQueue;
        CommandQueue<QueueType::GRAPHICS, FRAME_CNT> m_graphicsCommandQueue;
        ComPtr<IDXGISwapChain3>                      m_swapChain;
        HANDLE                                       m_swapChainWaitableObject;
        DescriptorPool<DescType::RTV>                m_rtvPool;
        ComPtr<ID3D12Resource>                       m_renderTargets[BUF_CNT];
        DescriptorPool<DescType::DSV>                m_dsvPool;
        ComPtr<ID3D12Resource>                       m_depthBuffer;
        /* Pipeline objects */
        UploadRingBuffer                             m_uploadBuffer;
        ComPtr<ID3D12GraphicsCommandList>            m_copyCommandList;
        ConstantBuffer                               m_constantBuffer;
        ComPtr<ID3D12RootSignature>                  m_graphicsRootSignature;
        ComPtr<ID3D12PipelineState>                  m_graphicsPipelineState;
        ComPtr<ID3D12GraphicsCommandList>            m_graphicsCommandList;
    };
} // namespace D3D12
