#pragma once

#include <dxgi1_4.h>
#include "HelperStructs.h"
#include "..\Common\Constants.h"

namespace D3D12 {
    class Renderer {
    public:
        Renderer();
        RULE_OF_ZERO_MOVE_ONLY(Renderer);
        // Creates a root signature according to its description
        ComPtr<ID3D12RootSignature>
        createRootSignature(const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const;
        // Creates a graphics pipeline state object (PSO) according to its description
        ComPtr<ID3D12PipelineState>
        createGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& stateDesc) const;
        // Creates a graphics command list in the specified initial state
        ComPtr<ID3D12GraphicsCommandList>
        createGraphicsCommandList(ID3D12PipelineState* const initialState);
        // Creates a vertex buffer for the vertex array with the specified number of vertices
        VertexBuffer createVertexBuffer(const uint count, const Vertex* const vertices) const;
        // Creates an index buffer for the index array with the specified number of indices
        IndexBuffer createIndexBuffer(const uint count, const uint* const indices) const;
        // Initializes the frame rendering process
        void startFrame();
        // Draws the geometry from the indexed vertex buffer to the frame buffer
        void draw(const VertexBuffer& vbo, const IndexBuffer& ibo);
        // Finalizes the frame rendering process
        void finalizeFrame();
        // Finishes the current frame and stops the execution
        void stop();
    private:
        // Configures the rendering pipeline, including the shaders
        void configurePipeline();
        // Creates a constant buffer of the specified size in bytes
        // Optionally, it also uploads the data to the device
        ComPtr<ID3D12Resource>
        createConstantBuffer(const uint size, const void* const data = nullptr) const;
    private:
        /* Rendering parameters */
        D3D12_VIEWPORT                    m_viewport;
        D3D12_RECT                        m_scissorRect;
        uint                              m_backBufferIndex;
        /* Direct3D resources */
        mutable ComPtr<ID3D12DeviceEx>    m_device;
        WorkQueue<WorkType::GRAPHICS>     m_graphicsWorkQueue;
        ComPtr<IDXGISwapChain3>           m_swapChain;
        ComPtr<ID3D12Resource>            m_renderTargets[BUFFER_COUNT];
        DescriptorPool<DescType::RTV>     m_rtvPool;
        ComPtr<ID3D12Resource>            m_depthBuffer;
        DescriptorPool<DescType::DSV>     m_dsvPool;
        UploadBuffer                      m_uploadBuffer;
        /* Pipeline objects */
        ComPtr<ID3D12RootSignature>       m_rootSignature;
        ComPtr<ID3D12PipelineState>       m_pipelineState;
        ComPtr<ID3D12GraphicsCommandList> m_graphicsCommandList;
    };
} // namespace D3D12
