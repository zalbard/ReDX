#pragma once

#include <dxgi1_4.h>
#include <DirectXMath.h>
#include "WorkManagement.h"

// Forward declarations
struct CD3DX12_ROOT_SIGNATURE_DESC;
struct D3D12_GRAPHICS_PIPELINE_STATE_DESC;

namespace D3D12 {
    // Simple vertex represenation
    struct Vertex {
        DirectX::XMFLOAT3 position;  // Homogeneous screen-space coordinates from [-0.5, 0.5]
        DirectX::XMFLOAT4 color;     // RGBA color
    };

    // Direct3D renderer
    class Renderer {
    public:
        Renderer() = delete;
        RULE_OF_ZERO_MOVE_ONLY(Renderer);
        // Constructor; takes horizontal and vertical resolution as input
        Renderer(const long resX, const long resY);
        // Creates a root signature according to its description
        ComPtr<ID3D12RootSignature> 
            createRootSignature(const CD3DX12_ROOT_SIGNATURE_DESC* const rootSignatureDesc);
        // Creates a graphics pieline state object (PSO) according to its description
        // A PSO describes the input data format, and how the data is processed (rendered)
        ComPtr<ID3D12PipelineState>
            createPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* const pipelineStateDesc);
        // Creates a graphics command list of a specified type, in a specified initial state
        ComPtr<ID3D12GraphicsCommandList>
            createGraphicsCmdList(ID3D12PipelineState* const initState);
        // Draws a single frame to the framebuffer
        void renderFrame();
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
        // Creates a descriptor heap
        void createDescriptorHeap();
        // Creates RTVs for each frame buffer
        void createRenderTargetViews();
        // Configures the rendering pipeline, including the shaders
        void configurePipeline();
        // Creates a vertex buffer
        void createVertexBuffer();
        // Resets and then populates the graphics command list
        void recordCommandList();
    private:
        static const uint                 m_singleGpuNodeMask = 0u;
        static const uint                 m_bufferCount       = 2u;
        static const bool                 m_useWarpDevice     = false;
        /* Rendering parameters */
        D3D12_VIEWPORT                    m_viewport;
        D3D12_RECT                        m_scissorRect;
        uint                              m_backBufferIndex;
        /* Pipeline objects */
        ComPtr<ID3D12Device>              m_device;
        GraphicsWorkQueue                 m_graphicsWorkQueue;
        ComPtr<IDXGISwapChain3>           m_swapChain;
        ComPtr<ID3D12Resource>            m_renderTargets[m_bufferCount];
        ComPtr<ID3D12DescriptorHeap>      m_rtvHeap;
        uint                              m_rtvDescriptorSize;
        /* Application resources */
        ComPtr<ID3D12RootSignature>       m_rootSignature;
        ComPtr<ID3D12PipelineState>       m_pipelineState;
        ComPtr<ID3D12GraphicsCommandList> m_graphicsCmdList;
        ComPtr<ID3D12Resource>            m_vertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW          m_vertexBufferView;
    };
} // namespace D3D12
