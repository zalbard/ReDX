#include <d3dcompiler.h>
#include "d3dx12.h"
#include "Renderer.h"
#include "..\UI\Window.h"
#include "..\Common\Utility.h"

using namespace D3D12;

// Prints the C string (errMsg) if the HRESULT (hr) fails
#define CHECK_CALL(hr, errMsg)     \
    if (FAILED(hr)) {              \
        printError(errMsg);        \
        panic(__FILE__, __LINE__); \
    }

Renderer::Renderer(const LONG resX, const LONG resY) {
    // Configure the viewport
    m_viewport = D3D12_VIEWPORT{
        /* TopLeftX */ 0.0f,
        /* TopLeftY */ 0.0f,
        /* Width */    static_cast<float>(resX),
        /* Height */   static_cast<float>(resY),
        /* MinDepth */ 0.0f,
        /* MaxDepth */ 1.0f
    };
    // Configure the scissor rectangle used for clipping
    m_scissorRect = D3D12_RECT{
        /* left */   0l,
        /* top */    0l, 
        /* right */  resX,
        /* bottom */ resY
    };
    // Open a window for rendering output
    Window::create(resX, resY, this);
    // Perform the initialization step
    configureEnvironment();
    // Set up the rendering pipeline
    configurePipeline();
}

// Enables the Direct3D debug layer
static inline void enableDebugLayer() {
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    CHECK_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)),
               "Failed to initialize the D3D debug layer.");
    debugController->EnableDebugLayer();
#endif
}

// Creates a DirectX Graphics Infrastructure (DXGI) 1.1 factory
static inline ComPtr<IDXGIFactory4> createDxgiFactory4() {
    // IDXGIFactory4 inherits from IDXGIFactory1 (4 -> 3 -> 2 -> 1)
    ComPtr<IDXGIFactory4> factory;
    CHECK_CALL(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),
               "Failed to create a DXGI 1.1 factory.");
    return factory;
}

// Disables transitions from the windowed to the fullscreen mode
static inline void disableFullscreen(IDXGIFactory4* const factory) {
    CHECK_CALL(factory->MakeWindowAssociation(Window::handle(), DXGI_MWA_NO_ALT_ENTER),
               "Failed to disable fullscreen transitions.");
}

void Renderer::configureEnvironment() {
    enableDebugLayer();
    auto factory = createDxgiFactory4();
    disableFullscreen(factory.Get());
    createDevice(factory.Get());
    createCommandQueue();
    createSwapChain(factory.Get());
    createDescriptorHeap();
    createRenderTargetViews();
    createCommandAllocator();
}

void Renderer::createDevice(IDXGIFactory4 * const factory) {
    #pragma warning(suppress: 4127)
    if (!m_useWarpDevice) {
        // Use a hardware display adapter
        createHardwareDevice(factory);
    } else {
        // Use a software display adapter
        createWarpDevice(factory);
    }
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
            // It does -> create a Direct3D device
            CHECK_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                         IID_PPV_ARGS(&m_device)),
                       "Failed to create a Direct3D device.");
            return;
        }
    }
}

void Renderer::createWarpDevice(IDXGIFactory4* const factory) {
    ComPtr<IDXGIAdapter> adapter;
    CHECK_CALL(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),
               "Failed to create a WARP adapter.");
    CHECK_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)),
               "Failed to create a Direct3D device.");
}

void Renderer::createCommandQueue() {
    // Fill out the command queue description
    const D3D12_COMMAND_QUEUE_DESC queueDesc = {
        /* Type */     D3D12_COMMAND_LIST_TYPE_DIRECT,
        /* Priority */ D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        /* Flags */    D3D12_COMMAND_QUEUE_FLAG_NONE,
        /* NodeMask */ m_singleGpuNodeMask
    };
    // Create a command queue
    CHECK_CALL(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)),
               "Failed to create a command queue.");
}

void Renderer::createSwapChain(IDXGIFactory4* const factory) {
    // Fill out the buffer description
    const DXGI_MODE_DESC bufferDesc = {
        /* Width */            static_cast<UINT>(m_scissorRect.right - m_scissorRect.left),
        /* Height */           static_cast<UINT>(m_scissorRect.bottom - m_scissorRect.top),
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
        /* OutputWindow */ Window::handle(),
        /* Windowed */     TRUE,
        /* SwapEffect */   DXGI_SWAP_EFFECT_FLIP_DISCARD,
        /* Flags */        0u
    };
    // Create a swap chain; it needs a command queue to flush the latter
    CHECK_CALL(factory->CreateSwapChain(m_commandQueue.Get(), &swapChainDesc,
                                        reinterpret_cast<IDXGISwapChain**>(m_swapChain.GetAddressOf())),
               "Failed to create a swap chain.");
    // Use it to set the current back buffer index
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void Renderer::createDescriptorHeap() {
    // Fill out the descriptor heap description
    const D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        /* Type */           D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        /* NumDescriptors */ m_bufferCount,
        /* Flags */          D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        /* NodeMask */       m_singleGpuNodeMask
    };
    // Create a descriptor heap
    CHECK_CALL(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)),
               "Failed to create a descriptor heap.");
    // Set the offset (increment size) for descriptor handles
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void Renderer::createRenderTargetViews() {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescHandle{m_rtvHeap->GetCPUDescriptorHandleForHeapStart()};
    // Create a Render Target View (RTV) for each frame buffer
    for (UINT bufferIndex = 0u; bufferIndex < m_bufferCount; ++bufferIndex) {
        CHECK_CALL(m_swapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(&m_renderTargets[bufferIndex])),
                   "Failed to aquire a swap chain buffer.");
        m_device->CreateRenderTargetView(m_renderTargets[bufferIndex].Get(), nullptr, rtvDescHandle);
        // Increment the descriptor pointer by the descriptor size
        rtvDescHandle.Offset(1, m_rtvDescriptorSize);
    }
}

void Renderer::createCommandAllocator() {
    CHECK_CALL(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                IID_PPV_ARGS(&m_commandAllocator)),
               "Failed to create a command allocator.");
}

void Renderer::configurePipeline() {
    createRootSignature();
    createPipelineStateObject();
    createCommandList();
    createVertexBuffer();
    createSyncPrims();
    // Wait for the setup to complete before continuing
    waitForPreviousFrame();
}

void Renderer::createRootSignature() {
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
    CHECK_CALL(m_device->CreateRootSignature(m_singleGpuNodeMask,
                                             signature->GetBufferPointer(),
                                             signature->GetBufferSize(),
                                             IID_PPV_ARGS(&m_rootSignature)),
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
    CHECK_CALL(D3DCompileFromFile(pathAndFileName, nullptr, nullptr, entryPoint,
                                  type, compileFlags, 0u, &shader, nullptr),
               "Failed to load and compile a shader.");
    return shader;
}

void Renderer::createPipelineStateObject() {
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
        /* NodeMask */              m_singleGpuNodeMask,
        /* CachedPSO */             D3D12_CACHED_PIPELINE_STATE{},
        /* Flags */                 D3D12_PIPELINE_STATE_FLAG_NONE
    };
    // Create a graphics pipeline state object
    CHECK_CALL(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)),
               "Failed to create a pipeline state object.");
}

void Renderer::createCommandList() {
    CHECK_CALL(m_device->CreateCommandList(m_singleGpuNodeMask, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           m_commandAllocator.Get(), m_pipelineState.Get(),
                                           IID_PPV_ARGS(&m_commandList)),
        "Failed to create a command list.");
    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    CHECK_CALL(m_commandList->Close(), "Failed to close the command list.");
}

void Renderer::createVertexBuffer() {
    // Define a screen-space triangle
    const float aspectRatio = m_viewport.Width / m_viewport.Height;
    const Vertex triangleVertices[] = {
        Vertex{ {   0.0f,  0.25f * aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        Vertex{ {  0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        Vertex{ { -0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
    };
    const UINT vertexBufferSize = sizeof(triangleVertices);
    // Use an upload heap to hold the vertex buffer
    /* TODO: inefficient, change this! */
    const auto heapProperties   = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_UPLOAD};
    const auto vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &vertexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nullptr, IID_PPV_ARGS(&m_vertexBuffer)),
               "Failed to create an upload heap.");
    // Aquire a CPU pointer to the buffer (subresource "subRes")
    UINT8* pVertexData;
    static constexpr UINT subRes = 0u;
    // Note: we don't intend to read from this resource on the CPU
    static const CD3DX12_RANGE emptyReadRange{0u, 0u};
    CHECK_CALL(m_vertexBuffer->Map(subRes, &emptyReadRange, reinterpret_cast<void**>(&pVertexData)),
               "Failed to map the vertex buffer.");
    // Copy the triangle data to the vertex buffer
    memcpy(pVertexData, triangleVertices, sizeof(triangleVertices));
    // Unmap the buffer
    m_vertexBuffer->Unmap(subRes, nullptr);
    // Initialize the vertex buffer view
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes  = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes    = vertexBufferSize;
}

void Renderer::createSyncPrims() {
    // Create a 0-initialized memory fence object
    CHECK_CALL(m_device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)),
               "Failed to create a memory fence object.");
    // Set the first valid fence value to 1
    m_fenceValue = 1u;
    // Create a synchronization event
    m_syncEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_syncEvent) {
        CHECK_CALL(HRESULT_FROM_WIN32(GetLastError()),
                   "Failed to create a synchronization event.");
    }
}

void Renderer::renderFrame() {
    // Record all the commands we need to render the scene into the command list
    recordCommandList();
    // Execute the command list
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    // Present the frame
    CHECK_CALL(m_swapChain->Present(1u, 0u), "Failed to display the frame buffer.");
    // Wait for frame rendering to finish
    waitForPreviousFrame();
}

void Renderer::recordCommandList() {
    // Command list -allocators- can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress
    CHECK_CALL(m_commandAllocator->Reset(), "Failed to reset the command allocator.");
    // However, when ExecuteCommandList() is called on a particular command list,
    // that command list -itself- can then be reset at any time (and must be before re-recording)
    CHECK_CALL(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()),
               "Failed to reset the graphics command list.");
    // Set the necessary state
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->RSSetViewports(1u, &m_viewport);
    m_commandList->RSSetScissorRects(1u, &m_scissorRect);
    // Transition the back buffer state: Presenting -> Render Target
    auto backBuffer = m_renderTargets[m_backBufferIndex].Get();
    auto barrier    = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
                      D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1u, &barrier);
    // Set the back buffer as the render target
    const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
                                                  static_cast<int>(m_backBufferIndex),
                                                  m_rtvDescriptorSize};
    m_commandList->OMSetRenderTargets(1u, &rtvHandle, FALSE, nullptr);
    // Record commands
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0u, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0u, 1u, &m_vertexBufferView);
    m_commandList->DrawInstanced(3u, 1u, 0u, 0u);
    // Transition the back buffer state: Render Target -> Presenting
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
              D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1u, &barrier);
    // Finalize the list
    CHECK_CALL(m_commandList->Close(), "Failed to close the command list.");
}

void Renderer::stop() {
    waitForPreviousFrame();
    CloseHandle(m_syncEvent);
}

/* TODO: waiting is inefficient, change this! */
void Renderer::waitForPreviousFrame() {
    // Insert the fence into the GPU command queue
    CHECK_CALL(m_commandQueue->Signal(m_fence.Get(), m_fenceValue),
               "Failed to update the value of the memory fence.");
    // Wait until the previous frame is finished
    if (m_fence->GetCompletedValue() < m_fenceValue) {
        CHECK_CALL(m_fence->SetEventOnCompletion(m_fenceValue, m_syncEvent),
                   "Failed to set an event to trigger upon the memory fence reaching a set value.");
        WaitForSingleObject(m_syncEvent, INFINITE);
    }
    // Increment the fence value and flip the frame buffer
    ++m_fenceValue;
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
}
