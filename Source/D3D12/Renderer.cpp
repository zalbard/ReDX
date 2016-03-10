#include <d3dcompiler.h>
#include <memory>
#include "HelperStructs.hpp"
#include "Renderer.h"
#include "..\Common\Math.h"
#include "..\ThirdParty\d3dx12.h"
#include "..\UI\Window.h"

using namespace D3D12;
using namespace DirectX;

static inline uint width(const D3D12_RECT& rect) {
    return static_cast<uint>(rect.right - rect.left);
}

static inline uint height(const D3D12_RECT& rect) {
    return static_cast<uint>(rect.bottom - rect.top);
}

static inline ComPtr<ID3D12DeviceEx> createWarpDevice(IDXGIFactory4* const factory) {
    ComPtr<IDXGIAdapter> adapter;
    CHECK_CALL(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),
               "Failed to create a WARP adapter.");
    ComPtr<ID3D12DeviceEx> device;
    CHECK_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)),
               "Failed to create a Direct3D device.");
    return device;
}

static inline ComPtr<ID3D12DeviceEx> createHardwareDevice(IDXGIFactory4* const factory) {
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
            ComPtr<ID3D12DeviceEx> device;
            CHECK_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                         IID_PPV_ARGS(&device)),
                       "Failed to create a Direct3D device.");
            return device;
        }
    }
}

Renderer::Renderer() {
    const long resX = Window::width();
    const long resY = Window::height();
    // Configure the viewport
    m_viewport = D3D12_VIEWPORT{
        /* TopLeftX */ 0.f,
        /* TopLeftY */ 0.f,
        /* Width */    static_cast<float>(resX),
        /* Height */   static_cast<float>(resY),
        /* MinDepth */ 0.f,
        /* MaxDepth */ 1.f
    };
    // Configure the scissor rectangle used for clipping
    m_scissorRect = D3D12_RECT{
        /* left */   0,
        /* top */    0, 
        /* right */  resX,
        /* bottom */ resY
    };
    // Enable the Direct3D debug layer
    #ifdef _DEBUG
    {
        ComPtr<ID3D12Debug> debugController;
        CHECK_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)),
                   "Failed to initialize the D3D debug layer.");
        debugController->EnableDebugLayer();
    }
    #endif
    // Create a DirectX Graphics Infrastructure (DXGI) 1.4 factory
    // IDXGIFactory4 inherits from IDXGIFactory1 (4 -> 3 -> 2 -> 1)
    ComPtr<IDXGIFactory4> factory;
    CHECK_CALL(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),
               "Failed to create a DXGI 1.4 factory.");
    // Disable transitions from the windowed to the fullscreen mode
    CHECK_CALL(factory->MakeWindowAssociation(Window::handle(), DXGI_MWA_NO_ALT_ENTER),
               "Failed to disable fullscreen transitions.");
    // Create a Direct3D device that represents the display adapter
    #pragma warning(suppress: 4127)
    if (USE_WARP_DEVICE) {
        // Use software rendering
        m_device = createWarpDevice(factory.Get());
    } else {
        // Use hardware acceleration
        m_device = createHardwareDevice(factory.Get());
    }
    // Create command queues
    m_device->createCommandQueue(&m_copyCommandQueue);
    m_device->createCommandQueue(&m_graphicsCommandQueue);
    // Create a buffer swap chain
    {
        // Fill out the multi-sampling parameters
        const DXGI_SAMPLE_DESC sampleDesc = {
            /* Count */   1,    // No multi-sampling
            /* Quality */ 0     // Default sampler
        };
        // Fill out the swap chain description
        const DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
            /* Width */       width(m_scissorRect),
            /* Height */      height(m_scissorRect),
            /* Format */      RTV_FORMAT,
            /* Stereo */      0,
            /* SampleDesc */  sampleDesc,
            /* BufferUsage */ DXGI_USAGE_RENDER_TARGET_OUTPUT,
            /* BufferCount */ BUF_CNT,
            /* Scaling */     DXGI_SCALING_NONE,
            /* SwapEffect */  DXGI_SWAP_EFFECT_FLIP_DISCARD,
            /* AlphaMode */   DXGI_ALPHA_MODE_UNSPECIFIED,
            /* Flags */       DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
        };
        // Create a swap chain; it needs a command queue to flush the latter
        auto swapChainAddr = reinterpret_cast<IDXGISwapChain1**>(m_swapChain.GetAddressOf());
        CHECK_CALL(factory->CreateSwapChainForHwnd(m_graphicsCommandQueue.get(), Window::handle(),
                                                   &swapChainDesc, nullptr, nullptr, swapChainAddr),
                   "Failed to create a swap chain.");
        // Set the maximal rendering queue depth
        CHECK_CALL(m_swapChain->SetMaximumFrameLatency(FRAME_CNT),
                   "Failed to set the maximal frame latency of the swap chain.");
        // Retrieve the object used to wait for the swap chain
        m_swapChainWaitableObject = m_swapChain->GetFrameLatencyWaitableObject();
        // Block the thread until the swap chain is ready accept a new frame
        WaitForSingleObject(m_swapChainWaitableObject, INFINITE);
    }
    // Create a render target view (RTV) descriptor pool
    m_device->createDescriptorPool(&m_rtvPool, BUF_CNT);
    // Create 2x RTVs
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{m_rtvPool.cpuBegin};
        // Create an RTV for each frame buffer
        for (uint bufferIdx = 0; bufferIdx < BUF_CNT; ++bufferIdx) {
            CHECK_CALL(m_swapChain->GetBuffer(bufferIdx, IID_PPV_ARGS(&m_renderTargets[bufferIdx])),
                       "Failed to acquire a swap chain buffer.");
            m_device->CreateRenderTargetView(m_renderTargets[bufferIdx].Get(), nullptr, rtvHandle);
            // Get the handle of the next descriptor
            rtvHandle.Offset(1, m_rtvPool.handleIncrSz);
        }
    }
    // Create a depth stencil view (DSV) descriptor pool
    m_device->createDescriptorPool(&m_dsvPool, 1);
    // Create a depth buffer
    {
        const DXGI_SAMPLE_DESC sampleDesc = {
            /* Count */   1,            // No multi-sampling
            /* Quality */ 0             // Default sampler
        };
        const D3D12_RESOURCE_DESC bufferDesc = {
            /* Dimension */        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            /* Alignment */        0,   // Automatic
            /* Width */            width(m_scissorRect),
            /* Height */           height(m_scissorRect),
            /* DepthOrArraySize */ 1,
            /* MipLevels */        0,   // Automatic
            /* Format */           DSV_FORMAT,
            /* SampleDesc */       sampleDesc,
            /* Layout */           D3D12_TEXTURE_LAYOUT_UNKNOWN,
            /* Flags */            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        };
        const CD3DX12_CLEAR_VALUE clearValue{
            /* Format */  DSV_FORMAT,
            /* Depth */   0.f,
            /* Stencil */ 0
        };
        const CD3DX12_HEAP_PROPERTIES heapProperties{D3D12_HEAP_TYPE_DEFAULT};
        CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                     &bufferDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                     &clearValue, IID_PPV_ARGS(&m_depthBuffer)),
                   "Failed to allocate a depth buffer.");
        const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {
            /* Format */        DSV_FORMAT,
            /* ViewDimension */ D3D12_DSV_DIMENSION_TEXTURE2D,
            /* Flags */         D3D12_DSV_FLAG_NONE,
            /* Texture2D */     {}
        };
        m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, m_dsvPool.cpuBegin);
    }
    // Set up the rendering pipeline
    configurePipeline();
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

struct Shader {
    size_t                  size;
    std::unique_ptr<byte[]> bytecode;
};

static inline Shader loadShaderBytecode(const char* const pathAndFileName) {
    FILE* file;
    if (fopen_s(&file, pathAndFileName, "rb")) {
        printError("Shader file %s not found.", pathAndFileName);
        TERMINATE();
    }
    Shader shader;
    // Get the file size in bytes
    fseek(file, 0, SEEK_END);
    shader.size = ftell(file);
    rewind(file);
    // Read the file
    shader.bytecode = std::make_unique<byte[]>(shader.size);
    fread(shader.bytecode.get(), 1, shader.size, file);
    // Close the file and return the bytecode
    fclose(file);
    return shader;
}

void Renderer::configurePipeline() {
    // Create a persistently mapped buffer on the upload heap
    {
        m_uploadBuffer.capacity = UPLOAD_BUF_SIZE;
        // Allocate the buffer on the upload heap
        const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_UPLOAD};
        const auto bufferDesc     = CD3DX12_RESOURCE_DESC::Buffer(m_uploadBuffer.capacity);
        CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, 
                                                     &bufferDesc,
                                                     D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                     IID_PPV_ARGS(&m_uploadBuffer.resource)),
                   "Failed to allocate an upload buffer.");
        // Note: we don't intend to read from this resource on the CPU
        const D3D12_RANGE emptyReadRange{0, 0};
        // Map the buffer to a range in the CPU virtual address space
        CHECK_CALL(m_uploadBuffer.resource->Map(0, &emptyReadRange,
                                                reinterpret_cast<void**>(&m_uploadBuffer.begin)),
                   "Failed to map the upload buffer.");
    }
    // Create a copy command list in the default state
    CHECK_CALL(m_device->CreateCommandList(m_device->nodeMask, D3D12_COMMAND_LIST_TYPE_COPY,
                                           m_copyCommandQueue.listAlloca(), nullptr,
                                           IID_PPV_ARGS(&m_copyCommandList)),
               "Failed to create a copy command list.");
    // Create a graphics root signature
    {
        CD3DX12_ROOT_PARAMETER vertexCBV;
        vertexCBV.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
        // Fill out the root signature description
        const auto rootSigneDesc = D3D12_ROOT_SIGNATURE_DESC{1, &vertexCBV, 0, nullptr,
                                   D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
        // Serialize a root signature from the description
        ComPtr<ID3DBlob> signature, error;
        CHECK_CALL(D3D12SerializeRootSignature(&rootSigneDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                               &signature, &error),
                   "Failed to serialize a root signature.");
        // Create a root signature layout using the serialized signature
        CHECK_CALL(m_device->CreateRootSignature(m_device->nodeMask,
                                                 signature->GetBufferPointer(),
                                                 signature->GetBufferSize(),
                                                 IID_PPV_ARGS(&m_graphicsRootSignature)),
                   "Failed to create a graphics root signature.");
    }
    // Import the vertex and the pixel shaders
    const Shader vs = loadShaderBytecode("Shaders\\VSDraw.cso");
    const Shader ps = loadShaderBytecode("Shaders\\PSDraw.cso");
    // Configure the way depth and stencil tests affect stencil values
    const D3D12_DEPTH_STENCILOP_DESC depthStencilOpDesc = {
        /* StencilFailOp */      D3D12_STENCIL_OP_KEEP,
        /* StencilDepthFailOp */ D3D12_STENCIL_OP_KEEP,
        /* StencilPassOp  */     D3D12_STENCIL_OP_KEEP,
        /* StencilFunc */        D3D12_COMPARISON_FUNC_ALWAYS
    };
    // Fill out the depth stencil description
    const D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {
        /* DepthEnable */      TRUE,
        /* DepthWriteMask */   D3D12_DEPTH_WRITE_MASK_ALL,
        /* DepthFunc */        D3D12_COMPARISON_FUNC_GREATER,
        /* StencilEnable */    FALSE,
        /* StencilReadMask */  D3D12_DEFAULT_STENCIL_READ_MASK,
        /* StencilWriteMask */ D3D12_DEFAULT_STENCIL_WRITE_MASK,
        /* FrontFace */        depthStencilOpDesc,
        /* BackFace */         depthStencilOpDesc
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
            /* SemanticName */         "NORMAL",
            /* SemanticIndex */        0,
            /* Format */               DXGI_FORMAT_R32G32B32_FLOAT,
            /* InputSlot */            1,
            /* AlignedByteOffset */    0,
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
    // Configure the rasterizer state
    const D3D12_RASTERIZER_DESC rasterizerDesc = {
        /* FillMode */              D3D12_FILL_MODE_SOLID,
        /* CullMode */              D3D12_CULL_MODE_BACK,
        /* FrontCounterClockwise */ FALSE,
        /* DepthBias */             D3D12_DEFAULT_DEPTH_BIAS,
        /* DepthBiasClamp */        D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
        /* SlopeScaledDepthBias */  D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        /* DepthClipEnable */       TRUE,
        /* MultisampleEnable */     FALSE,
        /* AntialiasedLineEnable */ FALSE,
        /* ForcedSampleCount */     0,
        /* ConservativeRaster */    D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
    };
    // Fill out the pipeline state object description
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {
        /* pRootSignature */        m_graphicsRootSignature.Get(),
        /* VS */                    D3D12_SHADER_BYTECODE{
            /* pShaderBytecode */       vs.bytecode.get(),
            /* BytecodeLength  */       vs.size
                                    },
        /* PS */                    D3D12_SHADER_BYTECODE{
            /* pShaderBytecode */       ps.bytecode.get(),
            /* BytecodeLength  */       ps.size
                                    },
        /* DS, HS, GS */            {}, {}, {},
        /* StreamOutput */          D3D12_STREAM_OUTPUT_DESC{},
        /* BlendState */            CD3DX12_BLEND_DESC{D3D12_DEFAULT},
        /* SampleMask */            UINT_MAX,
        /* RasterizerState */       rasterizerDesc,
        /* DepthStencilState */     depthStencilDesc,
        /* InputLayout */           inputLayoutDesc,
        /* IBStripCutValue */       D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
        /* PrimitiveTopologyType */ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        /* NumRenderTargets */      1,
        /* RTVFormats[8] */         {RTV_FORMAT},
        /* DSVFormat */             DSV_FORMAT,
        /* SampleDesc */            sampleDesc,
        /* NodeMask */              m_device->nodeMask,
        /* CachedPSO */             D3D12_CACHED_PIPELINE_STATE{},
        /* Flags */                 D3D12_PIPELINE_STATE_FLAG_NONE
    };
    // Create the initial graphics pipeline state
    CHECK_CALL(m_device->CreateGraphicsPipelineState(&pipelineStateDesc,
                                                     IID_PPV_ARGS(&m_graphicsPipelineState)),
               "Failed to create a graphics pipeline state object.");
    // Create a graphics command list
    CHECK_CALL(m_device->CreateCommandList(m_device->nodeMask, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           m_graphicsCommandQueue.listAlloca(),
                                           m_graphicsPipelineState.Get(),
                                           IID_PPV_ARGS(&m_graphicsCommandList)),
               "Failed to create a graphics command list.");
    // Create a constant buffer
    m_constantBuffer = createConstantBuffer(sizeof(XMMATRIX));
}

ConstantBuffer Renderer::createConstantBuffer(const uint size, const void* const data) {
    assert(size >= 4);
    ConstantBuffer buffer;
    // Allocate the buffer on the default heap
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
    const auto bufferDesc     = CD3DX12_RESOURCE_DESC::Buffer(size);
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &bufferDesc, D3D12_RESOURCE_STATE_COMMON,
                                                 nullptr, IID_PPV_ARGS(&buffer.resource)),
               "Failed to allocate a constant buffer.");
    // Transition the memory buffer state for graphics/compute command queue type class
    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer.resource.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    m_graphicsCommandList->ResourceBarrier(1, &barrier);
    if (data) {
        // Copy the data to video memory using the upload buffer
        constexpr uint64 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        uploadData<alignment>(buffer, size, data);
    }
    // Initialize the constant buffer location
    buffer.location = buffer.resource->GetGPUVirtualAddress();
    return buffer;
}

IndexBuffer Renderer::createIndexBuffer(const uint count, const uint* const indices) {
    assert(indices && count >= 3);
    IndexBuffer buffer;
    const uint size = count * sizeof(uint);
    // Allocate the buffer on the default heap
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
    const auto bufferDesc     = CD3DX12_RESOURCE_DESC::Buffer(size);
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &bufferDesc, D3D12_RESOURCE_STATE_COMMON,
                                                 nullptr, IID_PPV_ARGS(&buffer.resource)),
               "Failed to allocate an index buffer.");
    // Transition the memory buffer state for graphics/compute command queue type class
    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer.resource.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    // Copy the indices to video memory using the upload buffer
    // Max. alignment requirement for indices is 4 bytes
    constexpr uint64 alignment = 4;
    uploadData<alignment>(buffer, size, indices);
    // Initialize the index buffer view
    buffer.view = D3D12_INDEX_BUFFER_VIEW{
        /* BufferLocation */ buffer.resource->GetGPUVirtualAddress(),
        /* SizeInBytes */    size,
        /* Format */         DXGI_FORMAT_R32_UINT
    };
    return buffer;
}

void Renderer::setViewProjMatrix(FXMMATRIX viewProjMat) {
    // Copy the data to video memory using the upload buffer
    constexpr uint64 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    const XMMATRIX transposedViewProj = XMMatrixTranspose(viewProjMat);
    uploadData<alignment>(m_constantBuffer, sizeof(transposedViewProj), &transposedViewProj);
}

template<uint64 alignment>
void Renderer::uploadData(MemoryBuffer& dst, const uint size, const void* const data) {
    assert(data && size > 0);
    // Compute the address within the upload buffer which we will copy the data to
    byte*  alignedAddress = align<alignment>(m_uploadBuffer.begin + m_uploadBuffer.offset);
    uint64 alignedOffset  = alignedAddress - m_uploadBuffer.begin;
    // Check whether the upload buffer has sufficient space left
    const int64 remainingCapacity = m_uploadBuffer.capacity - alignedOffset;
    const bool  wrapAround = remainingCapacity < size;
    if (wrapAround) {
        m_uploadBuffer.offset = 0;
        // Recompute 'alignedAddress' and 'alignedOffset'
        alignedAddress = align<alignment>(m_uploadBuffer.begin);
        alignedOffset  = alignedAddress - m_uploadBuffer.begin;
        // Make sure the upload buffer is sufficiently large
        #ifdef _DEBUG
        {
            const int64 alignedCapacity = m_uploadBuffer.capacity - alignedOffset;
            if (alignedCapacity < size) {
                printError("Insufficient upload buffer capacity: "
                           "current (aligned): %i, required: %u.",
                           alignedCapacity, size);
                TERMINATE();
            }
        }
        #endif
    }
    const uint alignedEnd = static_cast<uint>(alignedOffset + size);
    // Make sure we do not overwrite any data which is yet to be copied to the GPU
    // Case 1: |====OFFS====CURR====AEND====|
    const bool case1 = (wrapAround | (m_uploadBuffer.offset < m_uploadBuffer.currSegStart)) &
                       (m_uploadBuffer.currSegStart <= alignedEnd);
    // Cases 2-4 are possible only if there is a valid previous segment of the buffer
    const bool validPrevSeg = m_uploadBuffer.prevSegStart < UINT_MAX;
    // Case 2: |----CURR====PREV====AEND----|
    const bool case2 = (m_uploadBuffer.currSegStart < m_uploadBuffer.prevSegStart) &
                       (m_uploadBuffer.prevSegStart < alignedEnd);
    // Case 3: |====AEND----CURR====PREV====|
    const bool case3 = (alignedEnd < m_uploadBuffer.currSegStart) &
                       (m_uploadBuffer.currSegStart < m_uploadBuffer.prevSegStart);
    // Case 4: |====PREV====AEND----CURR====|
    const bool case4 = (m_uploadBuffer.prevSegStart < alignedEnd) &
                       (alignedEnd < m_uploadBuffer.currSegStart);
    if (case1 || (validPrevSeg && (case2 | case3 | case4))) {
        // In case 1, the buffer is full, so we have to block the thread
        executeCopyCommands(case1);
    }
    // Load the data into the upload buffer
    memcpy(alignedAddress, data, size);
    // Copy the data from the upload buffer into the memory buffer
    m_copyCommandList->CopyBufferRegion(dst.resource.Get(), 0,
                                        m_uploadBuffer.resource.Get(), alignedOffset, size);
    // Set the offset at the end of the data
    m_uploadBuffer.offset = alignedEnd;
}

void D3D12::Renderer::executeCopyCommands(const bool fullSync) {
    // Finalize and execute the command list
    CHECK_CALL(m_copyCommandList->Close(), "Failed to close the copy command list.");
    m_copyCommandQueue.execute(m_copyCommandList.Get());
    // Wait for the copy command list execution to finish
    auto fenceAndValue = m_copyCommandQueue.insertFence();
    m_copyCommandQueue.syncThread(fullSync ? fenceAndValue.second : 0);
    // Ensure synchronization between the graphics and the copy command queues
    m_graphicsCommandQueue.syncQueue(fenceAndValue.first, fenceAndValue.second);
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU
    CHECK_CALL(m_copyCommandQueue.listAlloca()->Reset(),
               "Failed to reset the copy command list allocator.");
    // After a command list has been executed, 
    // it can then be reset at any time (and must be before re-recording)
    CHECK_CALL(m_copyCommandList->Reset(m_copyCommandQueue.listAlloca(), nullptr),
               "Failed to reset the copy command list.");
    // Begin a new segment of the upload buffer
    m_uploadBuffer.prevSegStart = fullSync ? UINT_MAX : m_uploadBuffer.currSegStart;
    m_uploadBuffer.currSegStart = m_uploadBuffer.offset;
}

void Renderer::startFrame() {
    // Update the index of the frame buffer used for drawing
    m_backBufferIndex     = m_swapChain->GetCurrentBackBufferIndex();
    // Transition the back buffer state: Presenting -> Render Target
    const auto backBuffer = m_renderTargets[m_backBufferIndex].Get();
    const auto barrier    = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
                            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_graphicsCommandList->ResourceBarrier(1, &barrier);
    // Set the necessary state
    m_graphicsCommandList->SetGraphicsRootSignature(m_graphicsRootSignature.Get());
    m_graphicsCommandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer.location);
    m_graphicsCommandList->RSSetViewports(1, &m_viewport);
    m_graphicsCommandList->RSSetScissorRects(1, &m_scissorRect);
    // Set the back buffer as the render target
    const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{m_rtvPool.cpuBegin,
                                                  static_cast<int>(m_backBufferIndex),
                                                  m_rtvPool.handleIncrSz};
    const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle{m_dsvPool.cpuBegin};
    m_graphicsCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    // Clear the RTV and the DSV
    constexpr float clearColor[] = {0.f, 0.f, 0.f, 1.f};
    m_graphicsCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    const D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;
    m_graphicsCommandList->ClearDepthStencilView(dsvHandle, clearFlags, 0.f, 0, 0, nullptr);
    // Set the primitive/topology type
    m_graphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Renderer::finalizeFrame() {
    // Transition the back buffer state: Render Target -> Presenting
    const auto backBuffer = m_renderTargets[m_backBufferIndex].Get();
    const auto barrier    = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
                            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_graphicsCommandList->ResourceBarrier(1, &barrier);
    // Finalize and execute the command list
    CHECK_CALL(m_graphicsCommandList->Close(), "Failed to close the graphics command list.");
    m_graphicsCommandQueue.execute(m_graphicsCommandList.Get());
    // Present the frame
    CHECK_CALL(m_swapChain->Present(VSYNC_INTERVAL, 0), "Failed to display the frame buffer.");
    // Wait until all the previously issued commands have been executed
    m_graphicsCommandQueue.insertFence();
    m_graphicsCommandQueue.syncThread();
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU
    CHECK_CALL(m_graphicsCommandQueue.listAlloca()->Reset(),
               "Failed to reset the graphics command list allocator.");
    // After a command list has been executed, 
    // it can then be reset at any time (and must be before re-recording)
    CHECK_CALL(m_graphicsCommandList->Reset(m_graphicsCommandQueue.listAlloca(),
                                            m_graphicsPipelineState.Get()),
               "Failed to reset the graphics command list.");
    // Block the thread until the swap chain is ready accept a new frame
    // Otherwise, Present() may block the thread, increasing the input lag
    WaitForSingleObject(m_swapChainWaitableObject, INFINITE);
}

void Renderer::stop() {
    m_copyCommandQueue.finish();
    m_graphicsCommandQueue.finish();
}
