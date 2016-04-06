#include <d3dcompiler.h>
#include <d3dx12.h>
#include <tuple>
#include "HelperStructs.hpp"
#include "Renderer.hpp"
#include "..\Common\Buffer.h"
#include "..\Common\Math.h"
#include "..\UI\Window.h"

using namespace D3D12;
using namespace DirectX;

static inline auto createWarpDevice(IDXGIFactory4* factory)
-> ComPtr<ID3D12DeviceEx> {
    ComPtr<IDXGIAdapter> adapter;
    CHECK_CALL(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),
               "Failed to create a WARP adapter.");
    ComPtr<ID3D12DeviceEx> device;
    CHECK_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)),
               "Failed to create a Direct3D device.");
    return device;
}

static inline auto createHardwareDevice(IDXGIFactory4* factory)
-> ComPtr<ID3D12DeviceEx> {
    for (uint adapterIndex = 0; ; ++adapterIndex) {
        ComPtr<IDXGIAdapter1> adapter;
        if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &adapter)) {
            // No more adapters to enumerate.
            printError("Direct3D 12 device not found.");
            TERMINATE();
        }
        // Check whether the adapter supports Direct3D 12.
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                        _uuidof(ID3D12Device), nullptr))) {
            // It does -> create a Direct3D device.
            ComPtr<ID3D12DeviceEx> device;
            CHECK_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                         IID_PPV_ARGS(&device)),
                       "Failed to create a Direct3D device.");
            return device;
        }
    }
}

Renderer::Renderer()
    : m_frameIndex{0} {
    // Configure the scissor rectangle used for clipping.
    m_scissorRect = D3D12_RECT{
        /* left */   0,
        /* top */    0,
        /* right */  Window::width(),
        /* bottom */ Window::height()
    };
    // Configure the viewport.
    m_viewport = D3D12_VIEWPORT{
        /* TopLeftX */ 0.f,
        /* TopLeftY */ 0.f,
        /* Width */    static_cast<float>(width(m_scissorRect)),
        /* Height */   static_cast<float>(height(m_scissorRect)),
        /* MinDepth */ 0.f,
        /* MaxDepth */ 1.f
    };
    // Enable the Direct3D debug layer.
    #ifdef _DEBUG
    {
        ComPtr<ID3D12Debug> debugController;
        CHECK_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)),
                   "Failed to initialize the D3D debug layer.");
        debugController->EnableDebugLayer();
    }
    #endif
    // Create a DirectX Graphics Infrastructure (DXGI) 1.4 factory.
    // IDXGIFactory4 inherits from IDXGIFactory1 (4 -> 3 -> 2 -> 1).
    ComPtr<IDXGIFactory4> factory;
    CHECK_CALL(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),
               "Failed to create a DXGI 1.4 factory.");
    // Disable transitions from the windowed to the fullscreen mode.
    CHECK_CALL(factory->MakeWindowAssociation(Window::handle(), DXGI_MWA_NO_ALT_ENTER),
               "Failed to disable fullscreen transitions.");
    // Create a Direct3D device that represents the display adapter.
    #pragma warning(suppress: 4127)
    if (USE_WARP_DEVICE) {
        // Use software rendering.
        m_device = createWarpDevice(factory.Get());
    } else {
        // Use hardware acceleration.
        m_device = createHardwareDevice(factory.Get());
    }
    // Make sure the GPU time stamp counter does not stop ticking during idle periods.
    CHECK_CALL(m_device->SetStablePowerState(true),
               "Failed to enable the stable GPU power state.");
    // Create command contexts.
    m_device->createCommandContext(&m_copyContext);
    m_device->createCommandContext(&m_graphicsContext);
    // Create descriptor pools.
    m_device->createDescriptorPool(&m_rtvPool);
    m_device->createDescriptorPool(&m_dsvPool);
    m_device->createDescriptorPool(&m_texPool);
    // Create a buffer swap chain.
    {
        // Fill out the swap chain description.
        const DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
            /* Width */       width(m_scissorRect),
            /* Height */      height(m_scissorRect),
            /* Format */      RTV_FORMAT,
            /* Stereo */      0,
            /* SampleDesc */  defaultSampleDesc,
            /* BufferUsage */ DXGI_USAGE_RENDER_TARGET_OUTPUT,
            /* BufferCount */ BUF_CNT,
            /* Scaling */     DXGI_SCALING_NONE,
            /* SwapEffect */  DXGI_SWAP_EFFECT_FLIP_DISCARD,
            /* AlphaMode */   DXGI_ALPHA_MODE_UNSPECIFIED,
            /* Flags */       DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
        };
        // Create a swap chain for the window.
        m_swapChain = m_graphicsContext.createSwapChain(factory.Get(), Window::handle(),
                                                        swapChainDesc);
        // Set the maximal rendering queue depth.
        CHECK_CALL(m_swapChain->SetMaximumFrameLatency(FRAME_CNT),
                   "Failed to set the maximal frame latency of the swap chain.");
        // Retrieve the object used to wait for the swap chain.
        m_swapChainWaitableObject = m_swapChain->GetFrameLatencyWaitableObject();
        // Block the thread until the swap chain is ready accept a new frame.
        WaitForSingleObject(m_swapChainWaitableObject, INFINITE);
        // Update the index of the frame buffer used for rendering.
        m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    }
    // Create a render target view (RTV) for each frame buffer.
    for (uint i = 0; i < BUF_CNT; ++i) {
        CHECK_CALL(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])),
                   "Failed to acquire a swap chain buffer.");
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr,
                                         m_rtvPool.cpuHandle(m_rtvPool.size++));
    }
    // Create a depth-stencil view (DSV) for each frame.
    for (uint i = 0; i < FRAME_CNT; ++i) {
        const D3D12_RESOURCE_DESC resourceDesc = {
            /* Dimension */        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            /* Alignment */        0,   // Automatic
            /* Width */            width(m_scissorRect),
            /* Height */           height(m_scissorRect),
            /* DepthOrArraySize */ 1,
            /* MipLevels */        0,   // Automatic
            /* Format */           DSV_FORMAT,
            /* SampleDesc */       defaultSampleDesc,
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
                                                     &resourceDesc,
                                                     D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
                                                     IID_PPV_ARGS(&m_frameResouces[i].depthBuffer)),
                                                     "Failed to allocate a depth buffer.");
        const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {
            /* Format */        DSV_FORMAT,
            /* ViewDimension */ D3D12_DSV_DIMENSION_TEXTURE2D,
            /* Flags */         D3D12_DSV_FLAG_NONE,
            /* Texture2D */     {}
        };
        m_device->CreateDepthStencilView(m_frameResouces[i].depthBuffer.Get(), &dsvDesc,
                                         m_dsvPool.cpuHandle(m_dsvPool.size++));
    }
    // Create a persistently mapped buffer on the upload heap.
    {
        m_uploadBuffer.capacity   = UPLOAD_BUF_SIZE;
        // Allocate the buffer on the upload heap.
        const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_UPLOAD};
        const auto resourceDesc   = CD3DX12_RESOURCE_DESC::Buffer(m_uploadBuffer.capacity);
        CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, 
                                                     &resourceDesc,
                                                     D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                     IID_PPV_ARGS(&m_uploadBuffer.resource)),
                   "Failed to allocate an upload buffer.");
        // Note: we don't intend to read from this resource on the CPU.
        constexpr D3D12_RANGE emptyReadRange{0, 0};
        // Map the buffer to a range in the CPU virtual address space.
        CHECK_CALL(m_uploadBuffer.resource->Map(0, &emptyReadRange,
                                                reinterpret_cast<void**>(&m_uploadBuffer.begin)),
                   "Failed to map the upload buffer.");
    }
    // Set up the rendering pipeline.
    configurePipeline();
}

void Renderer::configurePipeline() {
    // Import the bytecode of the graphics root signature and the shaders.
    const Buffer rsByteCode{"Shaders\\DrawRS.cso"};
    const Buffer vsByteCode("Shaders\\DrawVS.cso");
    const Buffer psByteCode("Shaders\\DrawPS.cso");
    // Create a graphics root signature.
    CHECK_CALL(m_device->CreateRootSignature(m_device->nodeMask,
                                             rsByteCode.data(),
                                             rsByteCode.size,
                                             IID_PPV_ARGS(&m_graphicsRootSignature)),
               "Failed to create a graphics root signature.");
    // Configure the way depth and stencil tests affect stencil values.
    const D3D12_DEPTH_STENCILOP_DESC depthStencilOpDesc = {
        /* StencilFailOp */      D3D12_STENCIL_OP_KEEP,
        /* StencilDepthFailOp */ D3D12_STENCIL_OP_KEEP,
        /* StencilPassOp  */     D3D12_STENCIL_OP_KEEP,
        /* StencilFunc */        D3D12_COMPARISON_FUNC_ALWAYS
    };
    // Fill out the depth stencil description.
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
    // Define the vertex input layout.
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
        },
        {
            /* SemanticName */         "TEXCOORD",
            /* SemanticIndex */        0,
            /* Format */               DXGI_FORMAT_R32G32_FLOAT,
            /* InputSlot */            2,
            /* AlignedByteOffset */    0,
            /* InputSlotClass */       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            /* InstanceDataStepRate */ 0
        },

    };
    const D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {
        /* pInputElementDescs */ inputElementDescs,
        /* NumElements */        _countof(inputElementDescs)
    };
    // Configure the rasterizer state.
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
    // Fill out the pipeline state object description.
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {
        /* pRootSignature */        m_graphicsRootSignature.Get(),
        /* VS */                    D3D12_SHADER_BYTECODE{
            /* pShaderBytecode */       vsByteCode.data(),
            /* BytecodeLength  */       vsByteCode.size
                                    },
        /* PS */                    D3D12_SHADER_BYTECODE{
            /* pShaderBytecode */       psByteCode.data(),
            /* BytecodeLength  */       psByteCode.size
                                    },
        /* DS, HS, GS, SO */        {}, {}, {}, {},
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
        /* SampleDesc */            defaultSampleDesc,
        /* NodeMask */              m_device->nodeMask,
        /* CachedPSO */             D3D12_CACHED_PIPELINE_STATE{},
        /* Flags */                 D3D12_PIPELINE_STATE_FLAG_NONE
    };
    // Create the initial graphics pipeline state.
    CHECK_CALL(m_device->CreateGraphicsPipelineState(&pipelineStateDesc,
                                                     IID_PPV_ARGS(&m_graphicsPipelineState)),
               "Failed to create a graphics pipeline state object.");
    // Set the command list states.
    m_copyContext.resetCommandList(0, nullptr);
    m_graphicsContext.resetCommandList(0, m_graphicsPipelineState.Get());
    // Create a constant buffer for material indices.
    m_materialBuffer = createConstantBuffer(1024);
    // Create constant buffers for transformation matrices.
    for (uint i = 0; i < FRAME_CNT; ++i) {
        m_frameResouces[i].transformBuffer = createConstantBuffer(sizeof(XMMATRIX));
    }
}

IndexBuffer Renderer::createIndexBuffer(const uint count, const uint* indices) {
    assert(indices && count >= 3);
    IndexBuffer buffer;
    const uint size = count * sizeof(uint);
    // Allocate the buffer on the default heap.
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
    const auto resourceDesc   = CD3DX12_RESOURCE_DESC::Buffer(size);
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                                 nullptr, IID_PPV_ARGS(&buffer.resource)),
               "Failed to allocate an index buffer.");
    // Transition the buffer state for the graphics/compute command queue type class.
    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer.resource.Get(),
                                                   D3D12_RESOURCE_STATE_COMMON,
                                                   D3D12_RESOURCE_STATE_INDEX_BUFFER);
    // Max. alignment requirement for indices is 4 bytes.
    constexpr uint64 alignment = 4;
    // Copy indices into the upload buffer.
    const uint offset = copyToUploadBuffer<alignment>(size, indices);
    // Copy the data from the upload buffer into the video memory buffer.
    m_copyContext.commandList(0)->CopyBufferRegion(buffer.resource.Get(), 0,
                                                   m_uploadBuffer.resource.Get(), offset,
                                                   size);
    // Initialize the index buffer view.
    buffer.view.BufferLocation = buffer.resource->GetGPUVirtualAddress(),
    buffer.view.SizeInBytes    = size;
    buffer.view.Format         = DXGI_FORMAT_R32_UINT;
    return buffer;
}

ConstantBuffer Renderer::createConstantBuffer(const uint size, const void* data) {
    assert(!data || size >= 4);
    ConstantBuffer buffer;
    // Allocate the buffer on the default heap.
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
    const auto resourceDesc   = CD3DX12_RESOURCE_DESC::Buffer(size);
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                                 nullptr, IID_PPV_ARGS(&buffer.resource)),
               "Failed to allocate a constant buffer.");
    // Transition the buffer state for the graphics/compute command queue type class.
    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer.resource.Get(),
                                                   D3D12_RESOURCE_STATE_COMMON,
                                                   D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    m_graphicsContext.commandList(0)->ResourceBarrier(1, &barrier);
    if (data) {
        constexpr uint64 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        // Copy the data into the upload buffer.
        const uint offset = copyToUploadBuffer<alignment>(size, data);
        // Copy the data from the upload buffer into the video memory buffer.
        m_copyContext.commandList(0)->CopyBufferRegion(buffer.resource.Get(), 0,
                                                       m_uploadBuffer.resource.Get(), offset,
                                                       size);
    }
    // Initialize the constant buffer view.
    buffer.view = buffer.resource->GetGPUVirtualAddress();
    return buffer;
}

std::pair<Texture, uint> Renderer::createTexture2D(const D3D12_SUBRESOURCE_FOOTPRINT& footprint,
                                                   const uint mipCount, const void* data) {
    Texture texture;
    // Allocate the texture on the default heap.
    const D3D12_RESOURCE_DESC resourceDesc = {
        /* Dimension */        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        /* Alignment */        D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
        /* Width */            footprint.Width,
        /* Height */           footprint.Height,
        /* DepthOrArraySize */ static_cast<uint16>(footprint.Depth),
        /* MipLevels */        static_cast<uint16>(mipCount),
        /* Format */           footprint.Format,
        /* SampleDesc */       defaultSampleDesc,
        /* Layout */           D3D12_TEXTURE_LAYOUT_UNKNOWN,
        /* Flags */            D3D12_RESOURCE_FLAG_NONE
    };
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                                 nullptr, IID_PPV_ARGS(&texture.resource)),
               "Failed to allocate a texture.");
    // Transition the buffer state for the graphics/compute command queue type class.
    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.resource.Get(),
                                                   D3D12_RESOURCE_STATE_COMMON,
                                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_graphicsContext.commandList(0)->ResourceBarrier(1, &barrier);
    if (data) {
        constexpr uint64 pitchAlignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
        constexpr uint64 placeAlignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
        assert(0 == footprint.RowPitch % pitchAlignment);
        // Upload MIP levels one by one.
        for (uint i = 0; i < mipCount; ++i) {
            const uint rowPitch   = footprint.RowPitch >> i;
            const uint levelPitch = align<pitchAlignment>(rowPitch);
            const uint levelSize  = levelPitch * footprint.Height >> i;
            uint offset;
            // Check whether pitched copying is required.
            if (rowPitch == levelPitch) {
                // Copy the entire MIP level at once.
                offset = copyToUploadBuffer<placeAlignment>(levelSize, data);
                // Advance the data pointer to the next MIP level.
                data = static_cast<const byte*>(data) + levelSize;
            } else {
                // Reserve a chunk of memory for the entire MIP level.
                byte* address;
                std::tie(address, offset) = reserveChunkOfUploadBuffer<placeAlignment>(levelSize);
                // Copy the MIP level one row at a time.
                for (uint row = 0, height = footprint.Height >> i; row < height; ++row) {
                    memcpy(address, data, rowPitch);
                    address += levelPitch;
                    data     = static_cast<const byte*>(data) + rowPitch;
                }
            }
            // Copy the data from the upload buffer into the video memory texture.
            const D3D12_PLACED_SUBRESOURCE_FOOTPRINT levelFootprint = {
                /* Offset */   offset,
                /* Format */   footprint.Format,
                /* Width */    footprint.Width >> i,
                /* Height */   footprint.Height >> i,
                /* Depth */    footprint.Depth,
                /* RowPitch */ levelPitch
            };
            const CD3DX12_TEXTURE_COPY_LOCATION src{m_uploadBuffer.resource.Get(), levelFootprint};
            const CD3DX12_TEXTURE_COPY_LOCATION dst{texture.resource.Get(), i};
            m_copyContext.commandList(0)->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
    }
    // Describe the shader resource view.
    const CD3DX12_TEX2D_SRV_DESC srvDesc{footprint.Format, mipCount};
    // Initialize the shader resource view.
    const uint textureId = m_texPool.size++;
    texture.view = m_texPool.cpuHandle(textureId);
    m_device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, texture.view);
    return {texture, textureId};
}

void Renderer::setMaterials(const uint size, const void* data) {
    constexpr uint64 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    // Copy the data into the upload buffer.
    const uint offset = copyToUploadBuffer<alignment>(size, data);
    // Copy the data from the upload buffer into the video memory buffer.
    m_copyContext.commandList(0)->CopyBufferRegion(m_materialBuffer.resource.Get(), 0,
                                                   m_uploadBuffer.resource.Get(), offset,
                                                   size);
}

void Renderer::setViewProjMatrix(FXMMATRIX viewProj) {
    const XMMATRIX tViewProj = XMMatrixTranspose(viewProj);
    constexpr uint64 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    // Copy the data into the upload buffer.
    const uint offset = copyToUploadBuffer<alignment>(sizeof(tViewProj), &tViewProj);
    // Copy the data from the upload buffer into the video memory buffer.
    const ConstantBuffer& transformBuffer = m_frameResouces[m_frameIndex].transformBuffer;
    m_copyContext.commandList(0)->CopyBufferRegion(transformBuffer.resource.Get(), 0,
                                                   m_uploadBuffer.resource.Get(), offset,
                                                   sizeof(tViewProj));
}

void D3D12::Renderer::executeCopyCommands(const bool immediateCopy) {
    // Finalize and execute the command list.
    ID3D12Fence* insertedFence;
    uint64       insertedValue;
    std::tie(insertedFence, insertedValue) = m_copyContext.executeCommandList(0);
    // Ensure synchronization between the graphics and the copy command queues.
    m_graphicsContext.syncCommandQueue(insertedFence, insertedValue);
    if (immediateCopy) {
        printWarning("Immediate copy requested. Thread stall imminent.");
        m_copyContext.syncThread(insertedValue);
    } else {
        // For single- and double-buffered copy contexts, resetCommandAllocator() will
        // take care of waiting until the previous copy command list has completed execution.
        static_assert(decltype(m_copyContext)::bufferCount <= 2,
                      "Unsupported copy context buffering mode.");
    }
    // Reset the command list allocator.
    m_copyContext.resetCommandAllocator();
    // Reset the command list to its initial state.
    m_copyContext.resetCommandList(0, nullptr);
    // Begin a new segment of the upload buffer.
    m_uploadBuffer.prevSegStart = immediateCopy ? UINT_MAX : m_uploadBuffer.currSegStart;
    m_uploadBuffer.currSegStart = m_uploadBuffer.offset;
}

std::pair<uint64, uint64> Renderer::getTime() const {
    return m_graphicsContext.getTime();
}

void Renderer::startFrame() {
    // Transition the back buffer state: Presenting -> Render Target.
    const auto backBuffer = m_renderTargets[m_backBufferIndex].Get();
    const auto barrier    = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
                                                      D3D12_RESOURCE_STATE_PRESENT,
                                                      D3D12_RESOURCE_STATE_RENDER_TARGET);
    ID3D12GraphicsCommandList* graphicsCommandList = m_graphicsContext.commandList(0);
    graphicsCommandList->ResourceBarrier(1, &barrier);
    // Set the necessary state.
    graphicsCommandList->RSSetViewports(1, &m_viewport);
    graphicsCommandList->RSSetScissorRects(1, &m_scissorRect);
    ID3D12DescriptorHeap* texHeap = m_texPool.descriptorHeap();
    graphicsCommandList->SetDescriptorHeaps(1, &texHeap);
    // Configure the root signature.
    graphicsCommandList->SetGraphicsRootSignature(m_graphicsRootSignature.Get());
    const ConstantBuffer& transformBuffer = m_frameResouces[m_frameIndex].transformBuffer;
    graphicsCommandList->SetGraphicsRootConstantBufferView(1, transformBuffer.view);
    graphicsCommandList->SetGraphicsRootConstantBufferView(2, m_materialBuffer.view);
    graphicsCommandList->SetGraphicsRootDescriptorTable(3, m_texPool.gpuHandle(0));
    // Set the back buffer as the render target.
    const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvPool.cpuHandle(m_backBufferIndex);
    const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvPool.cpuHandle(m_frameIndex);
    graphicsCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    // Clear the RTV and the DSV.
    constexpr float clearColor[] = {0.f, 0.f, 0.f, 1.f};
    graphicsCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    const D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;
    graphicsCommandList->ClearDepthStencilView(dsvHandle, clearFlags, 0.f, 0, 0, nullptr);
    // Set the primitive/topology type.
    graphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Renderer::drawIndexed(const Scene::Objects& objects) {
    // Record commands into the command list.
    const auto graphicsCommandList = m_graphicsContext.commandList(0);
    graphicsCommandList->IASetVertexBuffers(0, 3, objects.vertexAttrBuffers.views);
    uint16 matId = UINT16_MAX;
    for (uint i = 0; i < objects.count; ++i) {
        if (objects.visibilityBits.testBit(i)) {
            if (matId != objects.materialIndices[i]) {
                matId  = objects.materialIndices[i];
                // Set the object's material index.
                graphicsCommandList->SetGraphicsRoot32BitConstant(0, matId, 0);
            }
            // Draw the object.
            const uint count = objects.indexBuffers.views[i].SizeInBytes / sizeof(uint);
            graphicsCommandList->IASetIndexBuffer(&objects.indexBuffers.views[i]);
            graphicsCommandList->DrawIndexedInstanced(count, 1, 0, 0, 0);
        }
    }
}

void Renderer::finalizeFrame() {
    // Transition the back buffer state: Render Target -> Presenting.
    const auto backBuffer = m_renderTargets[m_backBufferIndex].Get();
    const auto barrier    = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
                                                      D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                      D3D12_RESOURCE_STATE_PRESENT);
    m_graphicsContext.commandList(0)->ResourceBarrier(1, &barrier);
    // Finalize and execute the command list.
    m_graphicsContext.executeCommandList(0);
    // Present the frame, and update the index of the render (back) buffer.
    CHECK_CALL(m_swapChain->Present(VSYNC_INTERVAL, 0), "Failed to display the frame buffer.");
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    // Reset the graphics command (frame) allocator, and update the frame index.
    m_frameIndex = m_graphicsContext.resetCommandAllocator();
    // Reset the command list to its initial state.
    m_graphicsContext.resetCommandList(0, m_graphicsPipelineState.Get());
    // Block the thread until the swap chain is ready accept a new frame.
    // Otherwise, Present() may block the thread, increasing the input lag.
    WaitForSingleObject(m_swapChainWaitableObject, INFINITE);
}

void Renderer::stop() {
    m_copyContext.destroy();
    m_graphicsContext.destroy();
}
