#include <d3dcompiler.h>
#include "Renderer.h"
#include "HelperStructs.hpp"
#include "..\ThirdParty\d3dx12.h"
#include "..\UI\Window.h"

using namespace D3D12;

Renderer::Renderer() {
    const long resX = Window::width();
    const long resY = Window::height();
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
        /* left */   0,
        /* top */    0, 
        /* right */  resX,
        /* bottom */ resY
    };
    // Perform the Direct3D initialization step
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
    m_device->createWorkQueue(&m_graphicsWorkQueue);
    m_device->createDescriptorHeap(&m_rtvDescriptorHeap, m_bufferCount);
    createSwapChain(factory.Get());
    createRenderTargetViews();
}

void Renderer::createDevice(IDXGIFactory4* const factory) {
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
    for (uint adapterIndex = 0; ; ++adapterIndex) {
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

void Renderer::createSwapChain(IDXGIFactory4* const factory) {
    // Fill out the buffer description
    const DXGI_MODE_DESC bufferDesc = {
        /* Width */            static_cast<uint>(m_scissorRect.right - m_scissorRect.left),
        /* Height */           static_cast<uint>(m_scissorRect.bottom - m_scissorRect.top),
        /* RefreshRate */      DXGI_RATIONAL{},
        /* Format */           DXGI_FORMAT_R8G8B8A8_UNORM,
        /* ScanlineOrdering */ DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
        /* Scaling */          DXGI_MODE_SCALING_UNSPECIFIED
    };
    // Fill out the multi-sampling parameters
    const DXGI_SAMPLE_DESC sampleDesc = {
        /* Count */   1,    // No multi-sampling
        /* Quality */ 0     // Default sampler
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
        /* Flags */        0
    };
    // Create a swap chain; it needs a command queue to flush the latter
    auto swapChainAddr = reinterpret_cast<IDXGISwapChain**>(m_swapChain.GetAddressOf());
    CHECK_CALL(factory->CreateSwapChain(m_graphicsWorkQueue.commandQueue(), &swapChainDesc,
                                        swapChainAddr),
               "Failed to create a swap chain.");
}

void Renderer::createRenderTargetViews() {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{m_rtvDescriptorHeap.cpuBegin};
    // Create a Render Target View (RTV) for each frame buffer
    for (uint bufferIndex = 0; bufferIndex < m_bufferCount; ++bufferIndex) {
        CHECK_CALL(m_swapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(&m_renderTargets[bufferIndex])),
                   "Failed to acquire a swap chain buffer.");
        m_device->CreateRenderTargetView(m_renderTargets[bufferIndex].Get(),
                                         nullptr, rtvHandle);
        // Get the handle of the next descriptor
        rtvHandle.Offset(1, m_rtvDescriptorHeap.handleIncrSz);
    }
}

// Loads and compiles vertex and pixel shaders; takes the full path to the shader file,
// the name of the main (entry point) function, and the shader type
static inline ComPtr<ID3DBlob> loadAndCompileShader(const wchar_t* const pathAndFileName,
                                                    const char* const entryPoint,
                                                    const char* const type) {
    #ifdef _DEBUG
        // Enable debugging symbol information, and disable certain optimizations
        constexpr uint compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    #else
        constexpr uint compileFlags = 0;
    #endif
    // Load and compile the shader
    ComPtr<ID3DBlob> shader, errors;
    if (FAILED(D3DCompileFromFile(pathAndFileName, nullptr, nullptr, entryPoint,
                                  type, compileFlags, 0, &shader, &errors))) {
        printError("Failed to load and compile a shader.");
        printError(static_cast<const char* const>(errors->GetBufferPointer()));
        TERMINATE();
    }
    return shader;
}

void Renderer::configurePipeline() {
    // Fill out the root signature description
    const auto rootSignatureDesc = D3D12_ROOT_SIGNATURE_DESC{0, nullptr, 0, nullptr,
                                   D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    // Create a root signature
    m_rootSignature = createRootSignature(rootSignatureDesc);
    // Import the vertex and the pixel shaders
    const auto vs = loadAndCompileShader(L"Source\\Shaders\\shaders.hlsl", "VSMain", "vs_5_0");
    const auto ps = loadAndCompileShader(L"Source\\Shaders\\shaders.hlsl", "PSMain", "ps_5_0");
    // Fill out the depth stencil description
    const D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {
        /* DepthEnable */      FALSE,
        /* DepthWriteMask */   D3D12_DEPTH_WRITE_MASK_ZERO,
        /* DepthFunc */        D3D12_COMPARISON_FUNC_NEVER,
        /* StencilEnable */    FALSE,
        /* StencilReadMask */  0,
        /* StencilWriteMask */ 0,
        /* FrontFace */        D3D12_DEPTH_STENCILOP_DESC{},
        /* BackFace */         D3D12_DEPTH_STENCILOP_DESC{}
    };
    // Define the vertex input layout
    const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {
            /* SemanticName */         "POSITION",
            /* SemanticIndex */        0,
            /* Format */               DXGI_FORMAT_R32G32B32_FLOAT,
            /* InputSlot */            0,
            /* AlignedByteOffset */    0,
            /* InputSlotClass */       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            /* InstanceDataStepRate */ 0
        },
        {
            /* SemanticName */         "COLOR",
            /* SemanticIndex */        0,
            /* Format */               DXGI_FORMAT_R32G32B32_FLOAT,
            /* InputSlot */            0,
            /* AlignedByteOffset */    sizeof(Vertex::position),
            /* InputSlotClass */       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            /* InstanceDataStepRate */ 0
        }
    };
    const D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {
        /* pInputElementDescs */ inputElementDescs,
        /* NumElements */        _countof(inputElementDescs)
    };
    // Fill out the multi-sampling parameters
    const DXGI_SAMPLE_DESC sampleDesc = {
        /* Count */   1,    // No multi-sampling
        /* Quality */ 0     // Default sampler
    };
    // Fill out the pipeline state object description
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {
        /* pRootSignature */        m_rootSignature.Get(),
        /* VS */                    D3D12_SHADER_BYTECODE{
            /* pShaderBytecode */       vs->GetBufferPointer(),
            /* BytecodeLength  */       vs->GetBufferSize()
                                    },
        /* PS */                    D3D12_SHADER_BYTECODE{
            /* pShaderBytecode */       ps->GetBufferPointer(),
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
        /* NumRenderTargets */      1,
        /* RTVFormats[8] */         {DXGI_FORMAT_R8G8B8A8_UNORM},
        /* DSVFormat */             DXGI_FORMAT_UNKNOWN,
        /* SampleDesc */            sampleDesc,
        /* NodeMask */              m_device->nodeMask,
        /* CachedPSO */             D3D12_CACHED_PIPELINE_STATE{},
        /* Flags */                 D3D12_PIPELINE_STATE_FLAG_NONE
    };
    // Create a graphics command list and its initial state
    m_pipelineState       = createGraphicsPipelineState(pipelineStateDesc);
    m_graphicsCommandList = createGraphicsCommandList(m_pipelineState.Get());
}

ComPtr<ID3D12RootSignature>
Renderer::createRootSignature(const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) {
    // Serialize a root signature from the description
    ComPtr<ID3DBlob> signature, error;
    CHECK_CALL(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &signature, &error),
               "Failed to serialize a root signature.");
    // Create a root signature layout using the serialized signature
    ComPtr<ID3D12RootSignature> rootSignature;
    CHECK_CALL(m_device->CreateRootSignature(m_device->nodeMask,
                                             signature->GetBufferPointer(),
                                             signature->GetBufferSize(),
                                             IID_PPV_ARGS(&rootSignature)),
               "Failed to create a root signature.");
    return rootSignature;
}

ComPtr<ID3D12PipelineState>
Renderer::createGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& pipelineStateDesc) {
    ComPtr<ID3D12PipelineState> pipelineState;
    CHECK_CALL(m_device->CreateGraphicsPipelineState(&pipelineStateDesc,
                                                     IID_PPV_ARGS(&pipelineState)),
               "Failed to create a graphics pipeline state object.");
    return pipelineState;
}

ComPtr<ID3D12GraphicsCommandList>
Renderer::createGraphicsCommandList(ID3D12PipelineState* const initialState) {
    assert(initialState);
    ComPtr<ID3D12GraphicsCommandList> graphicsCommandList;
    CHECK_CALL(m_device->CreateCommandList(m_device->nodeMask, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           m_graphicsWorkQueue.listAlloca(), initialState,
                                           IID_PPV_ARGS(&graphicsCommandList)),
               "Failed to create a command list.");
    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    CHECK_CALL(graphicsCommandList->Close(), "Failed to close the command list.");
    return graphicsCommandList;
}


VertexBuffer Renderer::createVertexBuffer(const Vertex* const vertices, const uint count) {
    assert(vertices && count >= 3);
    VertexBuffer vertexBuffer;
    const uint vertexBufferSize = count * sizeof(Vertex);
    // Use an upload heap to hold the vertex buffer
    /* TODO: inefficient, change this! */
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_UPLOAD};
    const auto vertBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &vertBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nullptr, IID_PPV_ARGS(&vertexBuffer.resource)),
               "Failed to create an upload heap.");
    // Acquire a CPU pointer to the buffer (sub-resource "subRes")
    uint8* vertexData;
    constexpr uint subRes = 0;
    // Note: we don't intend to read from this resource on the CPU
    const D3D12_RANGE emptyReadRange{0, 0};
    CHECK_CALL(vertexBuffer.resource->Map(subRes, &emptyReadRange,
                                          reinterpret_cast<void**>(&vertexData)),
               "Failed to map the vertex buffer.");
    // Copy the triangle data to the vertex buffer
    memcpy(vertexData, vertices, vertexBufferSize);
    // Unmap the buffer
    vertexBuffer.resource->Unmap(subRes, nullptr);
    // Initialize the vertex buffer view
    vertexBuffer.view = D3D12_VERTEX_BUFFER_VIEW{
        /* BufferLocation */ vertexBuffer.resource->GetGPUVirtualAddress(),
        /* SizeInBytes */    vertexBufferSize,
        /* StrideInBytes */  sizeof(Vertex)
    };
    return vertexBuffer;
}

IndexBuffer Renderer::createIndexBuffer(const uint* const indices, const uint count) {
    assert(indices && count >= 3);
    IndexBuffer indexBuffer;
    const uint indexBufferSize = count * sizeof(uint);
    // Use an upload heap to hold the index buffer
    /* TODO: inefficient, change this! */
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_UPLOAD};
    const auto idxBufferDesc  = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &idxBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nullptr, IID_PPV_ARGS(&indexBuffer.resource)),
               "Failed to create an upload heap.");
    // Acquire a CPU pointer to the buffer (sub-resource "subRes")
    uint8* indexData;
    constexpr uint subRes = 0;
    // Note: we don't intend to read from this resource on the CPU
    const D3D12_RANGE emptyReadRange{0, 0};
    CHECK_CALL(indexBuffer.resource->Map(subRes, &emptyReadRange,
                                         reinterpret_cast<void**>(&indexData)),
               "Failed to map the vertex buffer.");
    // Copy the triangle data to the index buffer
    memcpy(indexData, indices, indexBufferSize);
    // Unmap the buffer
    indexBuffer.resource->Unmap(subRes, nullptr);
    // Initialize the index buffer view
    indexBuffer.view = D3D12_INDEX_BUFFER_VIEW{
        /* BufferLocation */ indexBuffer.resource->GetGPUVirtualAddress(),
        /* SizeInBytes */    indexBufferSize,
        /* Format */         DXGI_FORMAT_R32_UINT
    };
    return indexBuffer;
}

void Renderer::startNewFrame() {
    // Wait until all the previously issued commands have been executed
    /* TODO: waiting is inefficient, change this! */
    m_graphicsWorkQueue.waitForCompletion();
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU
    CHECK_CALL(m_graphicsWorkQueue.listAlloca()->Reset(),
               "Failed to reset the command list allocator.");
    // Get the index of the frame buffer used for drawing
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void Renderer::draw(const VertexBuffer& vbo) {
    // After a command list has been executed, 
    // it can then be reset at any time (and must be before re-recording)
    CHECK_CALL(m_graphicsCommandList->Reset(m_graphicsWorkQueue.listAlloca(),
                                            m_pipelineState.Get()),
               "Failed to reset the graphics command list.");
    // Set the necessary state
    m_graphicsCommandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_graphicsCommandList->RSSetViewports(1, &m_viewport);
    m_graphicsCommandList->RSSetScissorRects(1, &m_scissorRect);
    // Transition the back buffer state: Presenting -> Render Target
    auto backBuffer = m_renderTargets[m_backBufferIndex].Get();
    auto barrier    = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
                      D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_graphicsCommandList->ResourceBarrier(1, &barrier);
    // Set the back buffer as the render target
    const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{m_rtvDescriptorHeap.cpuBegin,
                                                  static_cast<int>(m_backBufferIndex),
                                                  m_rtvDescriptorHeap.handleIncrSz};
    m_graphicsCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    // Record the command list
    constexpr float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    m_graphicsCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_graphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_graphicsCommandList->IASetVertexBuffers(0, 1, &vbo.view);
    m_graphicsCommandList->DrawInstanced(3, 1, 0, 0);
    // Transition the back buffer state: Render Target -> Presenting
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
              D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_graphicsCommandList->ResourceBarrier(1, &barrier);
    // Finalize the list
    CHECK_CALL(m_graphicsCommandList->Close(), "Failed to close the command list.");
    // Execute the command list
    ID3D12CommandList* commandLists[] = {m_graphicsCommandList.Get()};
    m_graphicsWorkQueue.execute(commandLists);
    // Present the frame
    CHECK_CALL(m_swapChain->Present(1, 0), "Failed to display the frame buffer.");
}

void Renderer::stop() {
    m_graphicsWorkQueue.finish();
}
