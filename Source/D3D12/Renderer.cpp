#include "d3dx12.h"
#include "Renderer.h"
#include "..\Common\Utility.h"

using namespace D3D12;

// Prints C string (errMsg) if HRESULT (hr) fails
#define CHECK_CALL(hr, errMsg)     \
    if (FAILED(hr)) {              \
        printError(errMsg);        \
        panic(__FILE__, __LINE__); \
    }

Renderer::Renderer(const LONG resX, const LONG resY, const HWND wnd):
          m_windowHandle{wnd}, m_width{resX}, m_height{resY},
          m_aspectRatio{static_cast<float>(m_width) / static_cast<float>(m_height)} {
    // Set up rendering pipeline
    initPipeline();
}

void Renderer::initPipeline() {
    #ifdef _DEBUG
        // Enable Direct3D debug layer
        enableDebugLayer();
    #endif
    /* 1. Create DirectX Graphics Infrastructure (DXGI) 1.1 factory */
    // IDXGIFactory4 inherits from IDXGIFactory1 (4 -> 3 -> 2 -> 1)
    ComPtr<IDXGIFactory4> factory;
    CHECK_CALL(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),
               "Failed to create a DXGI 1.1 factory.");
    // Disable fullscreen transitions
    CHECK_CALL(factory->MakeWindowAssociation(m_windowHandle, DXGI_MWA_NO_ALT_ENTER),
               "Failed to disable fullscreen transitions.");
    /* 2. Init display adapter */
    #pragma warning(suppress: 4127)
    if (m_useWarpDevice) {
        // Use software display adapter
        createWarpDevice(factory.Get());
    }
    else {
        // Use hardware display adapter
        createHardwareDevice(factory.Get());
    }
    /* 3. Create command queue */
    createCommandQueue();
    /* 4. Create swap chain */
    createSwapChain(factory.Get());
    // Use it to set current frame index
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    /* 5. Create descriptor heap */
    createDescriptorHeap();
    // Set offset (increment size) for descriptor handles
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    /* 6. Create frame resources */
    createFrameResources();
    /* 7. Create command allocator */
    CHECK_CALL(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                IID_PPV_ARGS(&m_commandAllocator)),
               "Failed to create a command allocator.");
}

void Renderer::enableDebugLayer() const {
    // Enable Direct3D debug layer
    ComPtr<ID3D12Debug> debugController;
    CHECK_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)),
               "Failed to initialize the D3D debug layer.");
    debugController->EnableDebugLayer();
}

void Renderer::createWarpDevice(IDXGIFactory4* const factory) {
    ComPtr<IDXGIAdapter> adapter;
    CHECK_CALL(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),
               "Failed to create a WARP adapter.");
    // Create Direct3D device
    CHECK_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)),
               "Failed to create a Direct3D device.");
}

void Renderer::createHardwareDevice(IDXGIFactory4* const factory) {
    for (UINT adapterIndex = 0; ; ++adapterIndex) {
        // Select next adapter
        ComPtr<IDXGIAdapter1> adapter;
        if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &adapter)) {
            // No more adapters to enumerate
            printError("Direct3D 12 device not found.");
            TERMINATE();
        }
        // Check whether adapter supports Direct3D 12
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                        _uuidof(ID3D12Device), nullptr))) {
            // Create Direct3D device
            CHECK_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                         IID_PPV_ARGS(&m_device)),
                       "Failed to create a Direct3D device.");
            return;
        }
    }
}

void Renderer::createCommandQueue() {
    // Fill out command queue description
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    // Create command queue
    CHECK_CALL(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)),
               "Failed to create a command queue.");
}

void Renderer::createSwapChain(IDXGIFactory4* const factory) {
    // Fill out swap chain description
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount       = m_bufferCount;
    swapChainDesc.BufferDesc.Width  = m_width;
    swapChainDesc.BufferDesc.Height = m_height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect        = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.OutputWindow      = m_windowHandle;
    swapChainDesc.SampleDesc.Count  = 1;
    swapChainDesc.Windowed          = TRUE;
    // Create swap chain; it needs command queue to flush the latter
    CHECK_CALL(factory->CreateSwapChain(m_commandQueue.Get(), &swapChainDesc,
               reinterpret_cast<IDXGISwapChain**>(m_swapChain.GetAddressOf())),
               "Failed to create a swap chain.");
}

void Renderer::createDescriptorHeap() {
    // Fill out descriptor heap description
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = m_bufferCount;
    rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    // Create descriptor heap
    CHECK_CALL(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)),
               "Failed to create a descriptor heap.");
}

void D3D12::Renderer::createFrameResources() {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{m_rtvHeap->GetCPUDescriptorHandleForHeapStart()};

    for (UINT bufferIndex = 0; bufferIndex < m_bufferCount; ++bufferIndex) {
        // Create Render Target View (RTV) for each frame buffer
        CHECK_CALL(m_swapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(&m_renderTargets[bufferIndex])),
                   "Failed to aquire a swap chain buffer.");
        m_device->CreateRenderTargetView(m_renderTargets[bufferIndex].Get(), nullptr, rtvHandle);
        // Increment descriptor pointer by descriptor size
        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }
}
