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
        createRootSignature(const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc);
        // Creates a graphics pipeline state object (PSO) according to its description
        // A PSO describes the input data format, and how the data is processed (rendered)
        ComPtr<ID3D12PipelineState>
        createGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& pipelineStateDesc);
        // Creates a graphics command list in the specified initial state
        ComPtr<ID3D12GraphicsCommandList>
        createGraphicsCommandList(ID3D12PipelineState* const initialState);
        // Creates a vertex buffer for the vertex array with the specified number of vertices
        VertexBuffer createVertexBuffer(const Vertex* const vertices, const uint count);
        // Initializes the frame rendering process
        void startNewFrame();
        // Draws the geometry from the vertex buffer to the frame buffer
        void draw(const VertexBuffer& vbo);
        // Finishes the current frame and stops the execution
        void stop();
    private:
        // Configures the hardware and the software layers (infrastructure)
        // This step is independent from the rendering pipeline
        void configureEnvironment();
        // Creates a Direct3D device that represents the display adapter
        void createDevice(IDXGIFactory4* const factory);
        // Creates a hardware Direct3D device
        void createHardwareDevice(IDXGIFactory4* const factory);
        // Creates a WARP (software) Direct3D device
        void createWarpDevice(IDXGIFactory4* const factory);
        // Creates a swap chain
        void createSwapChain(IDXGIFactory4* const factory);
        // Creates RTVs for each frame buffer
        void createRenderTargetViews();
        // Configures the rendering pipeline, including the shaders
        void configurePipeline();
    private:
        // Double buffering is used: present the front, render to the back
        static constexpr uint             m_bufferCount   = 2;
        // Software rendering flag
        static constexpr bool             m_useWarpDevice = false;
        /* Rendering parameters */
        D3D12_VIEWPORT                    m_viewport;
        D3D12_RECT                        m_scissorRect;
        uint                              m_backBufferIndex;
        /* Pipeline objects */
        ComPtr<ID3D12DeviceEx>            m_device;
        WorkQueue<WorkType::GRAPHICS>     m_graphicsWorkQueue;
        ComPtr<IDXGISwapChain3>           m_swapChain;
        ComPtr<ID3D12Resource>            m_renderTargets[m_bufferCount];
        DescriptorHeap<DescType::RTV>     m_rtvDescriptorHeap;
        /* Application resources */
        ComPtr<ID3D12RootSignature>       m_rootSignature;
        ComPtr<ID3D12PipelineState>       m_pipelineState;
        ComPtr<ID3D12GraphicsCommandList> m_graphicsCommandList;
    };
} // namespace D3D12
