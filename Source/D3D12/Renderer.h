#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl\client.h>
#include <DirectXMath.h>
#include "..\Common\Definitions.h"

namespace D3D12 {
    using DirectX::XMFLOAT3;
    using DirectX::XMFLOAT4;
    using Microsoft::WRL::ComPtr;

    // Simple vertex represenation
    struct Vertex {
        XMFLOAT3 position;  // Homogeneous screen-space coordinates from [-0.5, 0.5]
        XMFLOAT4 color;     // RGBA color
    };

    // Direct3D renderer
    class Renderer {
    public:
        RULE_OF_ZERO_MOVE_ONLY(Renderer);
        // Constructor; takes horizontal and vertical resolution as input
        Renderer(const LONG resX, const LONG resY);
        // Draws a single frame to the framebuffer
        void renderFrame();
        // Waits for all GPU commands to finish
        void stop();
    private:
        // Configures the hardware and the software layers
        // This step is independent from the rendering pipeline
        void configureEnvironment();
        // Enables the Direct3D debug layer
        void enableDebugLayer() const;
        // Creates a WARP (software) Direct3D device
        void createWarpDevice(IDXGIFactory4* const factory);
        // Creates a hardware Direct3D device
        void createHardwareDevice(IDXGIFactory4* const factory);
        // Creates a command queue
        void createCommandQueue();
        // Creates a swap chain
        void createSwapChain(IDXGIFactory4* const factory);
        // Creates a descriptor heap
        void createDescriptorHeap();
        // Creates RTVs for each frame buffer
        void createRenderTargetViews();
        // Configures the rendering pipeline, including the shaders
        void configurePipeline();
        // Creates a root signature
        void createRootSignature();
        // Creates a graphics pieline state object which describes
        // the input data format and how its processed (rendered)
        void createPipelineStateObject();
        // Creates a command list
        void createCommandList();
        // Creates a vertex buffer
        void createVertexBuffer();
        // Creates synchronization primitives, such as
        // memory fences and synchronization events
        void createSyncPrims();
        // Resets and then populates the graphics command list
        void recordCommandList();
        // Waits for frame rendering to finish
        void waitForPreviousFrame();
        /* Private data members */
        static const UINT                 m_singleGpuNodeMask = 0u;
        static const UINT                 m_bufferCount       = 2u;
        static const bool                 m_useWarpDevice     = false;
        /* Pipeline objects */
        D3D12_VIEWPORT                    m_viewport;
        D3D12_RECT                        m_scissorRect;
        ComPtr<IDXGISwapChain3>           m_swapChain;
        ComPtr<ID3D12Device>              m_device;
        ComPtr<ID3D12Resource>            m_renderTargets[m_bufferCount];
        ComPtr<ID3D12CommandAllocator>    m_commandAllocator;
        ComPtr<ID3D12CommandQueue>        m_commandQueue;
        ComPtr<ID3D12GraphicsCommandList> m_commandList;
        ComPtr<ID3D12RootSignature>       m_rootSignature;
        ComPtr<ID3D12DescriptorHeap>      m_rtvHeap;
        ComPtr<ID3D12PipelineState>       m_pipelineState;
        UINT                              m_rtvDescriptorSize;
        /* Application resources */
        ComPtr<ID3D12Resource>            m_vertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW          m_vertexBufferView;
        /* Synchronization objects */
        UINT                              m_backBufferIndex;
        HANDLE                            m_syncEvent;
        ComPtr<ID3D12Fence>               m_fence;
        UINT64                            m_fenceValue;
    };
} // namespace D3D12
