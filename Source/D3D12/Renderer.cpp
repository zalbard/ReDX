#include <d3dcompiler.h>
#include "Renderer.h"
#include "d3dx12.h"
#include "..\Common\Utility.h"

using namespace D3D12;

// Prints the C string (errMsg) if the HRESULT (hr) fails
#define CHECK_CALL(hr, errMsg)     \
    if (FAILED(hr)) {              \
        printError(errMsg);        \
        panic(__FILE__, __LINE__); \
    }

Renderer::Renderer(const LONG resX, const LONG resY, const HWND wnd):
          m_windowHandle{wnd}, m_width{resX}, m_height{resY},
          m_aspectRatio{static_cast<float>(m_width) / static_cast<float>(m_height)} {
    // Perform the initialization step
    configureEnvironment();
    // Set up the rendering pipeline
    configurePipeline();
}

void Renderer::configureEnvironment() {
    #ifdef _DEBUG
        // Enable the Direct3D debug layer
        enableDebugLayer();
    #endif
    /* 1. Create a DirectX Graphics Infrastructure (DXGI) 1.1 factory */
    // IDXGIFactory4 inherits from IDXGIFactory1 (4 -> 3 -> 2 -> 1)
    ComPtr<IDXGIFactory4> factory;
    CHECK_CALL(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),
               "Failed to create a DXGI 1.1 factory.");
    // Disable fullscreen transitions
    CHECK_CALL(factory->MakeWindowAssociation(m_windowHandle, DXGI_MWA_NO_ALT_ENTER),
               "Failed to disable fullscreen transitions.");
    /* 2. Initialize the display adapter */
    #pragma warning(suppress: 4127)
    if (m_useWarpDevice) {
        // Use a software display adapter
        createWarpDevice(factory.Get());
    }
    else {
        // Use a hardware display adapter
        createHardwareDevice(factory.Get());
    }
    /* 3. Create a command queue */
    createCommandQueue();
    /* 4. Create a swap chain */
    createSwapChain(factory.Get());
    // Use it to set the current frame index
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    /* 5. Create a descriptor heap */
    createDescriptorHeap();
    // Set the offset (increment size) for descriptor handles
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    /* 6. Create RTVs for each frame buffer */
    createRenderTargetViews();
    /* 7. Create a command allocator */
    CHECK_CALL(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                IID_PPV_ARGS(&m_commandAllocator)),
               "Failed to create a command allocator.");
}

void Renderer::enableDebugLayer() const {
    ComPtr<ID3D12Debug> debugController;
    CHECK_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)),
               "Failed to initialize the D3D debug layer.");
    debugController->EnableDebugLayer();
}

void Renderer::createWarpDevice(IDXGIFactory4* const factory) {
    ComPtr<IDXGIAdapter> adapter;
    CHECK_CALL(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),
               "Failed to create a WARP adapter.");
    // Create a Direct3D device
    CHECK_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)),
               "Failed to create a Direct3D device.");
}

void Renderer::createHardwareDevice(IDXGIFactory4* const factory) {
    for (UINT adapterIndex = 0u; ; ++adapterIndex) {
        // Select the next adapter
        ComPtr<IDXGIAdapter1> adapter;
        if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &adapter)) {
            // No more adapters to enumerate
            printError("Direct3D 12 device not found.");
            TERMINATE();
        }
        // Check whether the adapter supports Direct3D 12
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                        _uuidof(ID3D12Device), nullptr))) {
            // Create a Direct3D device
            CHECK_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                         IID_PPV_ARGS(&m_device)),
                       "Failed to create a Direct3D device.");
            return;
        }
    }
}

void Renderer::createCommandQueue() {
    // Fill out the command queue description
    const D3D12_COMMAND_QUEUE_DESC queueDesc = {
        /* Type */     D3D12_COMMAND_LIST_TYPE_DIRECT,
        /* Priority */ D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        /* Flags */    D3D12_COMMAND_QUEUE_FLAG_NONE,
        /* NodeMask */ 0u   //  Single GPU
    };
    // Create a command queue
    CHECK_CALL(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)),
               "Failed to create a command queue.");
}

void Renderer::createSwapChain(IDXGIFactory4* const factory) {
    // Fill out the buffer description
    const DXGI_MODE_DESC bufferDesc = {
        /* Width */            static_cast<UINT>(m_width),
        /* Height */           static_cast<UINT>(m_height),
        /* RefreshRate */      DXGI_RATIONAL{},
        /* Format */           DXGI_FORMAT_R8G8B8A8_UNORM,
        /* ScanlineOrdering */ DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
        /* Scaling */          DXGI_MODE_SCALING_UNSPECIFIED
    };
    // Fill out the multi-sampling parameters
    const DXGI_SAMPLE_DESC sampleDesc = {
        /* Count */   1u,   // No multi-sampling
        /* Quality */ 0u    // Default sampler
    };
    // Fill out the swap chain description
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {
        /* BufferDesc */   bufferDesc,
        /* SampleDesc */   sampleDesc,
        /* BufferUsage */  DXGI_USAGE_RENDER_TARGET_OUTPUT,
        /* BufferCount */  m_bufferCount,
        /* OutputWindow */ m_windowHandle,
        /* Windowed */     TRUE,
        /* SwapEffect */   DXGI_SWAP_EFFECT_FLIP_DISCARD,
        /* Flags */        0u
    };
    // Create a swap chain; it needs a command queue to flush the latter
    CHECK_CALL(factory->CreateSwapChain(m_commandQueue.Get(), &swapChainDesc,
               reinterpret_cast<IDXGISwapChain**>(m_swapChain.GetAddressOf())),
               "Failed to create a swap chain.");
}

void Renderer::createDescriptorHeap() {
    // Fill out the descriptor heap description
    const D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        /* Type */           D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        /* NumDescriptors */ m_bufferCount,
        /* Flags */          D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        /* NodeMask */       0u     //  Single GPU
    };
    // Create a descriptor heap
    CHECK_CALL(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)),
               "Failed to create a descriptor heap.");
}

void D3D12::Renderer::createRenderTargetViews() {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{m_rtvHeap->GetCPUDescriptorHandleForHeapStart()};
    // Create a Render Target View (RTV) for each frame buffer
    for (UINT bufferIndex = 0u; bufferIndex < m_bufferCount; ++bufferIndex) {
        CHECK_CALL(m_swapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(&m_renderTargets[bufferIndex])),
                   "Failed to aquire a swap chain buffer.");
        m_device->CreateRenderTargetView(m_renderTargets[bufferIndex].Get(), nullptr, rtvHandle);
        // Increment the descriptor pointer by the descriptor size
        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }
}

void D3D12::Renderer::configurePipeline() {
    /* 1. Create a root signature */
    createRootSignature();
    /* 2. Create a pipeline state object */
    createPipelineStateObject();
}

void D3D12::Renderer::createRootSignature() {
    // Fill out the root signature description
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(0u, nullptr, 0u, nullptr,
                           D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    // Serialize a root signature from the description
    ComPtr<ID3DBlob> signature, error;
    CHECK_CALL(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &signature, &error),
               "Failed to serialize a root signature.");
    // Create a root signature layout using the serialized signature
    CHECK_CALL(m_device->CreateRootSignature(0, signature->GetBufferPointer(),
               signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)),
               "Failed to create a root signature.");
}

// Loads and compiles vertex and pixel shaders; takes the full path to the shader file,
// the name of the main (entry point) function, and the shader type
ComPtr<ID3DBlob> loadAndCompileShader(const WCHAR* const pathAndFileName,
                                      const char* const entryPoint, const char* const type) {
    ComPtr<ID3DBlob> shader;
    #ifdef _DEBUG
        // Enable debugging symbol information, and disable certain optimizations
        const UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    #else
        const UINT compileFlags = 0;
    #endif
    // Load and compile the shader
    CHECK_CALL(D3DCompileFromFile(pathAndFileName, nullptr, nullptr,
               entryPoint, type, compileFlags, 0u, &shader, nullptr),
               "Failed to load and compile a shader.");
    return shader;
}

void D3D12::Renderer::createPipelineStateObject() {
    const auto vs = loadAndCompileShader(L"Source\\Shaders\\shaders.hlsl", "VSMain", "vs_5_0");
    const auto ps = loadAndCompileShader(L"Source\\Shaders\\shaders.hlsl", "PSMain", "ps_5_0");
    // Fill out the depth stencil description
    const D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {
        /* DepthEnable */      FALSE,
        /* DepthWriteMask */   D3D12_DEPTH_WRITE_MASK_ZERO,
        /* DepthFunc */        D3D12_COMPARISON_FUNC_NEVER,
        /* StencilEnable */    FALSE,
        /* StencilReadMask */  0u,
        /* StencilWriteMask */ 0u,
        /* FrontFace */        D3D12_DEPTH_STENCILOP_DESC{},
        /* BackFace */         D3D12_DEPTH_STENCILOP_DESC{}
    };
    // Define the vertex input layout
    const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {
            /* SemanticName */         "POSITION",
            /* SemanticIndex */        0u,
            /* Format */               DXGI_FORMAT_R32G32B32_FLOAT,
            /* InputSlot */            0u,
            /* AlignedByteOffset */    0u,
            /* InputSlotClass */       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            /* InstanceDataStepRate */ 0u
        },
        {
            /* SemanticName */         "COLOR",
            /* SemanticIndex */        0u,
            /* Format */               DXGI_FORMAT_R32G32B32_FLOAT,
            /* InputSlot */            0u,
            /* AlignedByteOffset */    12u,
            /* InputSlotClass */       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            /* InstanceDataStepRate */ 0u
        }
    };
    const D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {
        /* pInputElementDescs */ inputElementDescs,
        /* NumElements */        _countof(inputElementDescs)
    };
    // Fill out the multi-sampling parameters
    const DXGI_SAMPLE_DESC sampleDesc = {
        /* Count */   1u,   // No multi-sampling
        /* Quality */ 0u    // Default sampler
    };
    // Fill out the pipeline state object description
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {
        /* pRootSignature */        m_rootSignature.Get(),
        /* VS */                    D3D12_SHADER_BYTECODE{
            /* pShaderBytecode */       reinterpret_cast<UINT8*>(vs->GetBufferPointer()),
            /* BytecodeLength  */       vs->GetBufferSize()
                                    },
        /* PS */                    D3D12_SHADER_BYTECODE{
            /* pShaderBytecode */       reinterpret_cast<UINT8*>(ps->GetBufferPointer()),
            /* BytecodeLength  */       ps->GetBufferSize()
                                    },
        /* DS, HS, GS */            {}, {}, {},
        /* StreamOutput */          D3D12_STREAM_OUTPUT_DESC{},
        /* BlendState */            CD3DX12_BLEND_DESC{D3D12_DEFAULT},
        /* SampleMask */            UINT_MAX,
        /* RasterizerState */       CD3DX12_RASTERIZER_DESC{D3D12_DEFAULT},
        /* DepthStencilState */     depthStencilDesc,
        /* InputLayout */           inputLayoutDesc,
        /* IBStripCutValue */       D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
        /* PrimitiveTopologyType */ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        /* NumRenderTargets */      1u,
        /* RTVFormats[ 8 ] */       {DXGI_FORMAT_R8G8B8A8_UNORM},
        /* DSVFormat */             DXGI_FORMAT_UNKNOWN,
        /* SampleDesc */            sampleDesc,
        /* NodeMask */              0u,
        /* CachedPSO */             D3D12_CACHED_PIPELINE_STATE{},
        /* Flags */                 D3D12_PIPELINE_STATE_FLAG_NONE
    };
    // Create a graphics pipeline state object
    CHECK_CALL(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)),
               "Failed to create a pipeline state object.");
}
