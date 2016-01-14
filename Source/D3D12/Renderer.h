#pragma once

#include <dxgi1_4.h>
#include "HelperStructs.h"

namespace D3D12 {
    // Direct3D renderer
    class Renderer {
    public:
        Renderer();
        RULE_OF_ZERO_MOVE_ONLY(Renderer);
        // Creates a root signature according to its description
        ComPtr<ID3D12RootSignature>
        createRootSignature(const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const;
        // Creates a graphics pipeline state object (PSO) according to its description
        // A PSO describes the input data format, and how the data is processed (rendered)
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
        void startNewFrame();
        // Draws the geometry from the indexed vertex buffer to the frame buffer
        void draw(const VertexBuffer& vbo, const IndexBuffer& ibo);
        // Finishes the current frame and stops the execution
        void stop();
    private:
        // Configures the rendering pipeline, including the shaders
        void configurePipeline();
        // Creates a constant buffer of the specified size in bytes
        // Optionally, it also uploads the data to the device
        ComPtr<ID3D12Resource>
        createConstantBuffer(const uint byteSz, const void* const data = nullptr) const;
    private:
        // Double buffering is used: present the front, render to the back
        static constexpr uint             m_bufferCount   = 2;
        // Software rendering flag
        static constexpr bool             m_useWarpDevice = false;
        /* Rendering parameters */
        D3D12_VIEWPORT                    m_viewport;
        D3D12_RECT                        m_scissorRect;
        uint                              m_backBufferIndex;
        /* Direct3D resources */
        mutable ComPtr<ID3D12DeviceEx>    m_device;
        WorkQueue<WorkType::GRAPHICS>     m_graphicsWorkQueue;
        ComPtr<IDXGISwapChain3>           m_swapChain;
        ComPtr<ID3D12Resource>            m_renderTargets[m_bufferCount];
        DescriptorHeap<DescType::RTV>     m_rtvDescriptorHeap;
        /* Pipeline objects */
        ComPtr<ID3D12RootSignature>       m_rootSignature;
        ComPtr<ID3D12PipelineState>       m_pipelineState;
        ComPtr<ID3D12GraphicsCommandList> m_graphicsCommandList;
    };
} // namespace D3D12
