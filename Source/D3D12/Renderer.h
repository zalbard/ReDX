#pragma once

#include <DirectXMathSSE4.h>
#include "HelperStructs.h"
#include "..\Common\Constants.h"
#include "..\Common\DynBitSet.h"

namespace D3D12 {
    class Renderer {
    public:
        RULE_OF_ZERO_MOVE_ONLY(Renderer);
        Renderer();
        // Creates a vertex attribute buffer for the vertex array of 'count' elements.
        template <typename T>
        VertexBuffer createVertexBuffer(const uint count, const T* const elements);
        // Creates an index buffer for the index array with the specified number of indices.
        IndexBuffer createIndexBuffer(const uint count, const uint* const indices);
        // Creates a constant buffer for the data of the specified size in bytes.
        ConstantBuffer createConstantBuffer(const uint size, const void* const data = nullptr);
        // Sets the view-projection matrix in the shaders.
        void setViewProjMatrix(DirectX::FXMMATRIX viewProjMat);
        // Submits all pending copy commands for execution, and begins a new segment
        // of the upload buffer. As a result, the previous segment of the buffer becomes
        // available for writing. 'immediateCopy' flag ensures that all copies from the
        // current segment are also completed during this call (at the cost of blocking
        // the thread), therefore making the entire buffer free and available for writing.
        void executeCopyCommands(const bool immediateCopy = false);
        // Initializes the frame rendering process.
        void startFrame();
        template <uint N>
        // Draws geometry using 'N' vertex attribute buffers and 'iboCount' index buffers
        // 'drawMask' indicates whether geometry (within 'ibos') should be drawn.
        void drawIndexed(const VertexBuffer (&vbos)[N],
                         const IndexBuffer* const ibos, const uint iboCount,
                         const DynBitSet& drawMask);
        // Finalizes the frame rendering process.
        void finalizeFrame();
        // Terminates the rendering process.
        void stop();
    private:
        // Configures the rendering pipeline, including the shaders.
        void configurePipeline();
        // Copies the data of the specified size in bytes and alignment into the upload buffer.
        // Returns the offset into the upload buffer which corresponds to the location of the data.
        template<uint64 alignment>
        uint64 copyToUploadBuffer(const uint size, const void* const data);
    private:
        /* Rendering parameters */
        ComPtr<ID3D12DeviceEx>            m_device;
        D3D12_VIEWPORT                    m_viewport;
        D3D12_RECT                        m_scissorRect;
        GraphicsContext<FRAME_CNT, 1>     m_graphicsContext;
        ComPtr<ID3D12Resource>            m_renderTargets[BUF_CNT];
        uint                              m_backBufferIndex;
        ComPtr<ID3D12Resource>            m_depthBuffer;
        DescriptorPool<DescType::RTV>     m_rtvPool;
        DescriptorPool<DescType::DSV>     m_dsvPool;
        ComPtr<IDXGISwapChain3>           m_swapChain;
        HANDLE                            m_swapChainWaitableObject;
        CopyContext<2, 1>                 m_copyContext;
        UploadRingBuffer                  m_uploadBuffer;
        /* Pipeline objects */
        ConstantBuffer                    m_constantBuffer;
        ComPtr<ID3D12RootSignature>       m_graphicsRootSignature;
        ComPtr<ID3D12PipelineState>       m_graphicsPipelineState;
    };
} // namespace D3D12
