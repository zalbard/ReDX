#include <algorithm>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <tuple>
#include "Renderer.hpp"
#include "..\Common\Buffer.h"
#include "..\Common\Camera.h"
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
    for (uint32_t adapterIndex = 0; ; ++adapterIndex) {
        ComPtr<IDXGIAdapter1> adapter;
        if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &adapter)) {
            // No more adapters to enumerate.
            printError("Direct3D 12 device not found.");
            TERMINATE();
        }
        // Query the adapter info.
        DXGI_ADAPTER_DESC adapterDesc;
        adapter->GetDesc(&adapterDesc);
        // Skip the Intel GPU.
        if (adapterDesc.VendorId == 0x8086) {
            continue;
        }
        // Check whether the adapter supports the required D3D feature level.
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                        IID_PPV_ARGS(static_cast<ID3D12Device**>(nullptr))))) {
            // It does -> create a Direct3D device.
            ComPtr<ID3D12DeviceEx> device;
            CHECK_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                         IID_PPV_ARGS(&device)),
                       "Failed to create a Direct3D device.");
            // Convert the wide description string.
            char adapterDescString[32];
            wcstombs_s(nullptr, adapterDescString, 32, adapterDesc.Description, 31);
            // Print the graphics adapter details.
            printInfo("Graphics adapter: %s",      adapterDescString);
            printInfo("- Vendor id:      %u",      adapterDesc.VendorId);
            printInfo("- Device id:      %u",      adapterDesc.DeviceId);
            printInfo("- Dedicated VRAM: %zu MiB", adapterDesc.DedicatedVideoMemory / 1048576);
            printInfo("- Dedicated RAM:  %zu MiB", adapterDesc.DedicatedSystemMemory / 1048576);
            printInfo("- Shared RAM:     %zu MiB", adapterDesc.SharedSystemMemory / 1048576);
            return device;
        }
    }
}

Renderer::Renderer() {
    const uint32_t width  = Window::width();
    const uint32_t height = Window::height();
    // Configure the scissor rectangle used for clipping.
    m_scissorRect = D3D12_RECT{
        /* left */     0,
        /* top */      0,
        /* right */    static_cast<long>(width),
        /* bottom */   static_cast<long>(height)
    };
    // Configure the viewport.
    m_viewport = D3D12_VIEWPORT{
        /* TopLeftX */ 0.f,
        /* TopLeftY */ 0.f,
        /* Width */    static_cast<float>(width),
        /* Height */   static_cast<float>(height),
        /* MinDepth */ 0.f,
        /* MaxDepth */ 1.f
    };
    // Enable the Direct3D debug layer.
    #ifndef NDEBUG
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
            /* Width */       width,
            /* Height */      height,
            /* Format */      FORMAT_SC,
            /* Stereo */      0,
            /* SampleDesc */  SINGLE_SAMPLE,
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
    for (uint32_t i = 0; i < BUF_CNT; ++i) {
        constexpr D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {
            /* Format */        FORMAT_RTV,
            /* ViewDimension */ D3D12_RTV_DIMENSION_TEXTURE2D,
            /* MipSlice */      0,
            /* PlaneSlice */    0
        };
        CHECK_CALL(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_swapChainBuffers[i])),
                   "Failed to acquire a swap chain buffer.");
        m_device->CreateRenderTargetView(m_swapChainBuffers[i].Get(), &rtvDesc,
                                         m_rtvPool.cpuHandle(m_rtvPool.size++));
    }
    // Configure render passes.
    configureGBufferPass();
    configureShadingPass();
    // Set the initial command list states.
    m_copyContext.resetCommandList(0, nullptr);
    m_graphicsContext.resetCommandList(0, m_gBufferPass.pipelineState.Get());
    m_graphicsContext.resetCommandList(1, m_shadingPass.pipelineState.Get());
    // Create the G-buffer resources.
    {
        assert(m_dsvPool.size == 0);
        assert(m_rtvPool.size == BUF_CNT);
        // Create a depth buffer.
        m_gBuffer.depthBuffer   = createDepthBuffer(width, height, FORMAT_DSV);
        // Create a normal vector buffer.
        m_gBuffer.normalBuffer  = createRenderBuffer(width, height, FORMAT_NORMAL);
        // Create a UV coordinate buffer.
        m_gBuffer.uvCoordBuffer = createRenderBuffer(width, height, FORMAT_UVCOORD);
        // Create a UV gradient buffer.
        m_gBuffer.uvGradBuffer  = createRenderBuffer(width, height, FORMAT_UVGRAD);
        // Create a material ID buffer.
        m_gBuffer.matIdBuffer   = createRenderBuffer(width, height, FORMAT_MAT_ID);
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
    // Create a buffer for material indices.
    m_materialBuffer = createStructuredBuffer(MAT_CNT * sizeof(Material));
}

void D3D12::Renderer::configureGBufferPass() {
    auto& rootSignature = m_gBufferPass.rootSignature;
    auto& pipelineState = m_gBufferPass.pipelineState;
    // Import the bytecode of the graphics root signature and the shaders.
    const Buffer rsByteCode{"Shaders\\GBufferRS.cso"};
    const Buffer vsByteCode("Shaders\\GBufferVS.cso");
    const Buffer psByteCode("Shaders\\GBufferPS.cso");
    // Create a graphics root signature.
    CHECK_CALL(m_device->CreateRootSignature(m_device->nodeMask,
                                             rsByteCode.data(), rsByteCode.size,
                                             IID_PPV_ARGS(&rootSignature)),
               "Failed to create a graphics root signature.");
    // Configure the rasterizer state.
    constexpr D3D12_RASTERIZER_DESC rasterizerDesc = {
        /* FillMode */              D3D12_FILL_MODE_SOLID,
        /* CullMode */              D3D12_CULL_MODE_BACK,
        /* FrontCounterClockwise */ FALSE,
        /* DepthBias */             D3D12_DEFAULT_DEPTH_BIAS,
        /* DepthBiasClamp */        D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
        /* SlopeScaledDepthBias */  D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        /* DepthClipEnable */       FALSE,
        /* MultisampleEnable */     FALSE,
        /* AntialiasedLineEnable */ FALSE,
        /* ForcedSampleCount */     0,
        /* ConservativeRaster */    D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
    };
    // Configure the way depth and stencil tests affect stencil values.
    constexpr D3D12_DEPTH_STENCILOP_DESC depthStencilOpDesc = {
        /* StencilFailOp */         D3D12_STENCIL_OP_KEEP,
        /* StencilDepthFailOp */    D3D12_STENCIL_OP_KEEP,
        /* StencilPassOp  */        D3D12_STENCIL_OP_KEEP,
        /* StencilFunc */           D3D12_COMPARISON_FUNC_ALWAYS
    };
    // Fill out the depth stencil description.
    constexpr D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {
        /* DepthEnable */           TRUE,
        /* DepthWriteMask */        D3D12_DEPTH_WRITE_MASK_ALL,
        /* DepthFunc */             D3D12_COMPARISON_FUNC_GREATER,
        /* StencilEnable */         FALSE,
        /* StencilReadMask */       D3D12_DEFAULT_STENCIL_READ_MASK,
        /* StencilWriteMask */      D3D12_DEFAULT_STENCIL_WRITE_MASK,
        /* FrontFace */             depthStencilOpDesc,
        /* BackFace */              depthStencilOpDesc
    };
    // Define the vertex input layout.
    constexpr D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {
        /* SemanticName */          "Position",
        /* SemanticIndex */         0,
        /* Format */                DXGI_FORMAT_R32G32B32_FLOAT,
        /* InputSlot */             0,
        /* AlignedByteOffset */     0,
        /* InputSlotClass */        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        /* InstanceDataStepRate */  0
        },
        {
        /* SemanticName */          "Normal",
        /* SemanticIndex */         0,
        /* Format */                DXGI_FORMAT_R32G32B32_FLOAT,
        /* InputSlot */             1,
        /* AlignedByteOffset */     0,
        /* InputSlotClass */        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        /* InstanceDataStepRate */  0
        },
        {
        /* SemanticName */          "TexCoord",
        /* SemanticIndex */         0,
        /* Format */                DXGI_FORMAT_R32G32_FLOAT,
        /* InputSlot */             2,
        /* AlignedByteOffset */     0,
        /* InputSlotClass */        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        /* InstanceDataStepRate */  0
        }
    };
    constexpr D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {
        /* pInputElementDescs */    inputElementDescs,
        /* NumElements */           _countof(inputElementDescs)
    };
    // Fill out the pipeline state object description.
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {
        /* pRootSignature */        rootSignature.Get(),
        /* VS */                    {vsByteCode.data(), vsByteCode.size},
        /* PS */                    {psByteCode.data(), psByteCode.size},
        /* DS, HS, GS, SO */        {}, {}, {}, {},
        /* BlendState */            CD3DX12_BLEND_DESC{D3D12_DEFAULT},
        /* SampleMask */            UINT32_MAX,
        /* RasterizerState */       rasterizerDesc,
        /* DepthStencilState */     depthStencilDesc,
        /* InputLayout */           inputLayoutDesc,
        /* IBStripCutValue */       D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
        /* PrimitiveTopologyType */ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        /* NumRenderTargets */      4,
        /* RTVFormats[8] */         {FORMAT_NORMAL, FORMAT_UVCOORD, FORMAT_UVGRAD, FORMAT_MAT_ID},
        /* DSVFormat */             FORMAT_DSV,
        /* SampleDesc */            SINGLE_SAMPLE,
        /* NodeMask */              m_device->nodeMask,
        /* CachedPSO */             {},
        /* Flags */                 D3D12_PIPELINE_STATE_FLAG_NONE
    };
    // Create a graphics pipeline state object.
    CHECK_CALL(m_device->CreateGraphicsPipelineState(&pipelineStateDesc,
                                                     IID_PPV_ARGS(&pipelineState)),
               "Failed to create a graphics pipeline state object.");
}

void Renderer::configureShadingPass() {
    auto& rootSignature = m_shadingPass.rootSignature;
    auto& pipelineState = m_shadingPass.pipelineState;
    // Import the bytecode of the graphics root signature and the shaders.
    const Buffer rsByteCode{"Shaders\\ShadeRS.cso"};
    const Buffer vsByteCode("Shaders\\ShadeVS.cso");
    const Buffer psByteCode("Shaders\\ShadePS.cso");
    // Create a graphics root signature.
    CHECK_CALL(m_device->CreateRootSignature(m_device->nodeMask,
                                             rsByteCode.data(), rsByteCode.size,
                                             IID_PPV_ARGS(&rootSignature)),
               "Failed to create a graphics root signature.");
    // Configure the rasterizer state.
    constexpr D3D12_RASTERIZER_DESC rasterizerDesc = {
        /* FillMode */              D3D12_FILL_MODE_SOLID,
        /* CullMode */              D3D12_CULL_MODE_NONE,
        /* FrontCounterClockwise */ FALSE,
        /* DepthBias */             D3D12_DEFAULT_DEPTH_BIAS,
        /* DepthBiasClamp */        D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
        /* SlopeScaledDepthBias */  D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        /* DepthClipEnable */       FALSE,
        /* MultisampleEnable */     FALSE,
        /* AntialiasedLineEnable */ FALSE,
        /* ForcedSampleCount */     0,
        /* ConservativeRaster */    D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
    };
    // Fill out the pipeline state object description.
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {
        /* pRootSignature */        rootSignature.Get(),
        /* VS */                    {vsByteCode.data(), vsByteCode.size},
        /* PS */                    {psByteCode.data(), psByteCode.size},
        /* DS, HS, GS, SO */        {}, {}, {}, {},
        /* BlendState */            CD3DX12_BLEND_DESC{D3D12_DEFAULT},
        /* SampleMask */            UINT32_MAX,
        /* RasterizerState */       rasterizerDesc,
        /* DepthStencilState */     {},
        /* InputLayout */           {},
        /* IBStripCutValue */       D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
        /* PrimitiveTopologyType */ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        /* NumRenderTargets */      1,
        /* RTVFormats[8] */         {FORMAT_RTV},
        /* DSVFormat */             DXGI_FORMAT_UNKNOWN,
        /* SampleDesc */            SINGLE_SAMPLE,
        /* NodeMask */              m_device->nodeMask,
        /* CachedPSO */             {},
        /* Flags */                 D3D12_PIPELINE_STATE_FLAG_NONE
    };
    // Create a graphics pipeline state object.
    CHECK_CALL(m_device->CreateGraphicsPipelineState(&pipelineStateDesc,
                                                     IID_PPV_ARGS(&pipelineState)),
               "Failed to create a graphics pipeline state object.");
}

ComPtr<ID3D12Resource> Renderer::createDepthBuffer(const uint32_t width, const uint32_t height,
                                                   const DXGI_FORMAT format) {
    const D3D12_RESOURCE_DESC resourceDesc = {
        /* Dimension */        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        /* Alignment */        0,   // Automatic
        /* Width */            width,
        /* Height */           height,
        /* DepthOrArraySize */ 1,
        /* MipLevels */        1,
        /* Format */           getTypelessFormat(format),
        /* SampleDesc */       SINGLE_SAMPLE,
        /* Layout */           D3D12_TEXTURE_LAYOUT_UNKNOWN,
        /* Flags */            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    };
    const CD3DX12_CLEAR_VALUE clearValue = {
        /* Format */           format,
        /* Depth, Stencil */   0, 0
    };
    ComPtr<ID3D12Resource> depthStencilBuffer;
    // Allocate the depth buffer on the default heap.
    const CD3DX12_HEAP_PROPERTIES heapProperties{D3D12_HEAP_TYPE_DEFAULT};
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                 &clearValue, IID_PPV_ARGS(&depthStencilBuffer)),
               "Failed to allocate a depth buffer.");
    // Initialize the depth-stencil view.
    const D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {
        /* Format */           format,
        /* ViewDimension */    D3D12_DSV_DIMENSION_TEXTURE2D,
        /* Flags */            D3D12_DSV_FLAG_NONE,
        /* MipSlice */         0
    };
    m_device->CreateDepthStencilView(depthStencilBuffer.Get(), &dsvDesc,
                                     m_dsvPool.cpuHandle(m_dsvPool.size++));
    // Initialize the shader resource view.
    const D3D12_TEX2D_SRV_DESC srvDesc{getDepthSrvFormat(format), 1};
    m_device->CreateShaderResourceView(depthStencilBuffer.Get(), &srvDesc,
                                       m_texPool.cpuHandle(m_texPool.size++));
    return depthStencilBuffer;
}

ComPtr<ID3D12Resource> Renderer::createRenderBuffer(const uint32_t width, const uint32_t height,
                                                    const DXGI_FORMAT format) {
    const D3D12_RESOURCE_DESC resourceDesc = {
        /* Dimension */        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        /* Alignment */        0,   // Automatic,
        /* Width */            width,
        /* Height */           height,
        /* DepthOrArraySize */ 1,
        /* MipLevels */        1,
        /* Format */           format,
        /* SampleDesc */       SINGLE_SAMPLE,
        /* Layout */           D3D12_TEXTURE_LAYOUT_UNKNOWN,
        /* Flags */            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
    };
    const CD3DX12_CLEAR_VALUE clearValue = {
        /* Format */           format,
        /* Color */            FLOAT4_ZERO
    };
    ComPtr<ID3D12Resource> renderBuffer;
    // Allocate the render buffer on the default heap.
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &resourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                 &clearValue, IID_PPV_ARGS(&renderBuffer)),
               "Failed to allocate a render target.");
    // Initialize the render target view.
    const D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {
        /* Format */           format,
        /* ViewDimension*/     D3D12_RTV_DIMENSION_TEXTURE2D,
        /* MipSlice */         0,
        /* PlaneSlice */       0
    };
    m_device->CreateRenderTargetView(renderBuffer.Get(), &rtvDesc,
                                     m_rtvPool.cpuHandle(m_rtvPool.size++));
    // Initialize the shader resource view.
    const D3D12_TEX2D_SRV_DESC srvDesc{format, 1};
    m_device->CreateShaderResourceView(renderBuffer.Get(), &srvDesc,
                                       m_texPool.cpuHandle(m_texPool.size++));
    return renderBuffer;
}

Texture Renderer::createTexture2D(const D3D12_SUBRESOURCE_FOOTPRINT& footprint,
                                  const uint32_t mipCount, const void* data) {
    const D3D12_RESOURCE_DESC resourceDesc = {
        /* Dimension */        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        /* Alignment */        0,   // Automatic
        /* Width */            footprint.Width,
        /* Height */           footprint.Height,
        /* DepthOrArraySize */ static_cast<uint16_t>(footprint.Depth),
        /* MipLevels */        static_cast<uint16_t>(mipCount),
        /* Format */           footprint.Format,
        /* SampleDesc */       SINGLE_SAMPLE,
        /* Layout */           D3D12_TEXTURE_LAYOUT_UNKNOWN,
        /* Flags */            D3D12_RESOURCE_FLAG_NONE
    };
    const CD3DX12_CLEAR_VALUE clearValue = {
        /* Format */           footprint.Format,
        /* Color */            FLOAT4_ZERO
    };
    Texture texture;
    // Allocate the texture on the default heap.
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                                 nullptr, IID_PPV_ARGS(&texture.resource)),
               "Failed to allocate a texture.");
    // Transition the state of the texture for the graphics/compute command queue type class.
    const D3D12_TRANSITION_BARRIER barrier{texture.resource.Get(),
                                           D3D12_RESOURCE_STATE_COMMON,
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
    m_graphicsContext.commandList(0)->ResourceBarrier(1, &barrier);
    if (data) {
        assert(0 == footprint.RowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        // Upload MIP levels one by one.
        for (size_t i = 0; i < mipCount; ++i) {
            const uint32_t width     = std::max(1u, footprint.Width >> i);
            const uint32_t height    = std::max(1u, footprint.Height >> i);
            const size_t   dataPitch = std::max(1u, footprint.RowPitch >> i);
            const size_t   rowPitch  = align<D3D12_TEXTURE_DATA_PITCH_ALIGNMENT>(dataPitch);
            const size_t   size      = rowPitch * height;
            // Linear subresource copying must be aligned to 512 bytes.
            constexpr size_t alignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
            size_t offset;
            // Check whether pitched copying is required.
            if (dataPitch == rowPitch) {
                // Copy the entire MIP level at once.
                offset = copyToUploadBuffer<alignment>(size, data);
                // Advance the data pointer to the next MIP level.
                data = static_cast<const byte_t*>(data) + size;
            } else {
                // Reserve a chunk of memory for the entire MIP level.
                byte_t* address;
                std::tie(address, offset) = reserveChunkOfUploadBuffer<alignment>(size);
                // Copy the MIP level one row at a time.
                for (size_t row = 0; row < height; ++row) {
                    memcpy(address, data, dataPitch);
                    address += rowPitch;
                    data     = static_cast<const byte_t*>(data) + dataPitch;
                }
            }
            // Copy the data from the upload buffer into the video memory texture.
            const D3D12_PLACED_SUBRESOURCE_FOOTPRINT levelFootprint = {
                /* Offset */   offset,
                /* Format */   footprint.Format,
                /* Width */    width,
                /* Height */   height,
                /* Depth */    footprint.Depth,
                /* RowPitch */ static_cast<uint32_t>(rowPitch)
            };
            const uint32_t subResId = static_cast<uint32_t>(i);
            const CD3DX12_TEXTURE_COPY_LOCATION src{m_uploadBuffer.resource.Get(), levelFootprint};
            const CD3DX12_TEXTURE_COPY_LOCATION dst{texture.resource.Get(), subResId};
            m_copyContext.commandList(0)->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
    }
    // Initialize the shader resource view.
    const D3D12_TEX2D_SRV_DESC srvDesc{footprint.Format, mipCount};
    texture.view = m_texPool.gpuHandle(m_texPool.size);
    m_device->CreateShaderResourceView(texture.resource.Get(), &srvDesc,
                                       m_texPool.cpuHandle(m_texPool.size++));
    return texture;
}

size_t Renderer::getTextureIndex(const Texture& texture) const {
    return m_texPool.computeIndex(texture.view);
}

ConstantBuffer Renderer::createConstantBuffer(const size_t size, const void* data) {
    assert(!data || size >= 4);
    ConstantBuffer buffer;
    // Allocate the buffer on the default heap.
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
    const auto resourceDesc   = CD3DX12_RESOURCE_DESC::Buffer(size);
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                                 nullptr, IID_PPV_ARGS(&buffer.resource)),
               "Failed to allocate a constant buffer.");
    // Transition the state of the buffer for the graphics/compute command queue type class.
    const D3D12_TRANSITION_BARRIER barrier{buffer.resource.Get(),
                                           D3D12_RESOURCE_STATE_COMMON,
                                           D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER};
    m_graphicsContext.commandList(0)->ResourceBarrier(1, &barrier);
    if (data) {
        // Linear subresource copying must be aligned to 512 bytes.
        constexpr size_t alignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
        const     size_t offset    = copyToUploadBuffer<alignment>(size, data);
        // Copy the data from the upload buffer into the video memory buffer.
        m_copyContext.commandList(0)->CopyBufferRegion(buffer.resource.Get(), 0,
                                                       m_uploadBuffer.resource.Get(), offset,
                                                       size);
    }
    // Initialize the constant buffer view.
    buffer.view = buffer.resource->GetGPUVirtualAddress();
    return buffer;
}

StructuredBuffer Renderer::createStructuredBuffer(const size_t size, const void* data) {
    assert(!data || size >= 4);
    StructuredBuffer buffer;
    // Allocate the buffer on the default heap.
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
    const auto resourceDesc   = CD3DX12_RESOURCE_DESC::Buffer(size);
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                                 nullptr, IID_PPV_ARGS(&buffer.resource)),
               "Failed to allocate a structured buffer.");
    // Transition the state of the buffer for the graphics/compute command queue type class.
    const D3D12_TRANSITION_BARRIER barrier{buffer.resource.Get(),
                                           D3D12_RESOURCE_STATE_COMMON,
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
    m_graphicsContext.commandList(0)->ResourceBarrier(1, &barrier);
    if (data) {
        // Linear subresource copying must be aligned to 512 bytes.
        constexpr size_t alignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
        const     size_t offset    = copyToUploadBuffer<alignment>(size, data);
        // Copy the data from the upload buffer into the video memory buffer.
        m_copyContext.commandList(0)->CopyBufferRegion(buffer.resource.Get(), 0,
                                                       m_uploadBuffer.resource.Get(), offset,
                                                       size);
    }
    // Initialize the shader resource view.
    buffer.view = buffer.resource->GetGPUVirtualAddress();
    return buffer;
}

IndexBuffer Renderer::createIndexBuffer(const size_t count, const uint32_t* indices) {
    assert(indices && count >= 3);
    const size_t size = count * sizeof(uint32_t);
    IndexBuffer buffer;
    // Allocate the buffer on the default heap.
    const auto heapProperties = CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT};
    const auto resourceDesc   = CD3DX12_RESOURCE_DESC::Buffer(size);
    CHECK_CALL(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
                                                 &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                                 nullptr, IID_PPV_ARGS(&buffer.resource)),
               "Failed to allocate an index buffer.");
    // Transition the state of the buffer for the graphics/compute command queue type class.
    const D3D12_TRANSITION_BARRIER barrier{buffer.resource.Get(),
                                           D3D12_RESOURCE_STATE_COMMON,
                                           D3D12_RESOURCE_STATE_INDEX_BUFFER};
    m_graphicsContext.commandList(0)->ResourceBarrier(1, &barrier);
    // Linear subresource copying must be aligned to 512 bytes.
    constexpr size_t alignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
    const     size_t offset    = copyToUploadBuffer<alignment>(size, indices);
    // Copy the data from the upload buffer into the video memory buffer.
    m_copyContext.commandList(0)->CopyBufferRegion(buffer.resource.Get(), 0,
                                                   m_uploadBuffer.resource.Get(), offset,
                                                   size);
    // Initialize the index buffer view.
    buffer.view.BufferLocation = buffer.resource->GetGPUVirtualAddress(),
    buffer.view.SizeInBytes    = static_cast<uint32_t>(size);
    buffer.view.Format         = DXGI_FORMAT_R32_UINT;
    return buffer;
}

void Renderer::setMaterials(const size_t count, const Material* materials) {
    assert(count <= MAT_CNT);
    // Linear subresource copying must be aligned to 512 bytes.
    constexpr size_t alignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
    const     size_t size      = count * sizeof(Material);
    const     size_t offset    = copyToUploadBuffer<alignment>(size, materials);
    // Copy the data from the upload buffer into the video memory buffer.
    m_copyContext.commandList(0)->CopyBufferRegion(m_materialBuffer.resource.Get(), 0,
                                                   m_uploadBuffer.resource.Get(), offset, size);
}

void D3D12::Renderer::executeCopyCommands(const bool immediateCopy) {
    // Finalize and execute the command list.
    ID3D12Fence* insertedFence;
    uint64_t     insertedValue;
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
    m_copyContext.resetCommandAllocators();
    // Reset the command list to its initial state.
    m_copyContext.resetCommandList(0, nullptr);
    // Begin a new segment of the upload buffer.
    m_uploadBuffer.prevSegStart = immediateCopy ? m_uploadBuffer.offset
                                                : m_uploadBuffer.currSegStart;
    m_uploadBuffer.currSegStart = m_uploadBuffer.offset;
}

void Renderer::GBuffer::setWriteBarriers(D3D12_RESOURCE_BARRIER* barriers,                          
                                         const D3D12_RESOURCE_BARRIER_FLAGS flag) {
    barriers[0] = D3D12_TRANSITION_BARRIER{depthBuffer.Get(),
                                           D3D12_RESOURCE_STATE_DEPTH_READ |
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           D3D12_RESOURCE_STATE_DEPTH_WRITE,   flag};
    barriers[1] = D3D12_TRANSITION_BARRIER{normalBuffer.Get(),
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET, flag};
    barriers[2] = D3D12_TRANSITION_BARRIER{uvCoordBuffer.Get(),
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET, flag};
    barriers[3] = D3D12_TRANSITION_BARRIER{uvGradBuffer.Get(),
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET, flag};
    barriers[4] = D3D12_TRANSITION_BARRIER{matIdBuffer.Get(),
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET, flag};
}

void Renderer::GBuffer::setReadBarriers(D3D12_RESOURCE_BARRIER* barriers,                          
                                        const D3D12_RESOURCE_BARRIER_FLAGS flag) {
    barriers[0] = D3D12_TRANSITION_BARRIER{depthBuffer.Get(),
                                           D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                           D3D12_RESOURCE_STATE_DEPTH_READ |
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, flag};
    barriers[1] = D3D12_TRANSITION_BARRIER{normalBuffer.Get(),
                                           D3D12_RESOURCE_STATE_RENDER_TARGET,
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, flag};
    barriers[2] = D3D12_TRANSITION_BARRIER{uvCoordBuffer.Get(),
                                           D3D12_RESOURCE_STATE_RENDER_TARGET,
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, flag};
    barriers[3] = D3D12_TRANSITION_BARRIER{uvGradBuffer.Get(),
                                           D3D12_RESOURCE_STATE_RENDER_TARGET,
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, flag};
    barriers[4] = D3D12_TRANSITION_BARRIER{matIdBuffer.Get(),
                                           D3D12_RESOURCE_STATE_RENDER_TARGET,
                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, flag};
}

void Renderer::recordGBufferPass(const PerspectiveCamera& pCam, const Scene& scene) {
    ID3D12GraphicsCommandList* graphicsCommandList = m_graphicsContext.commandList(0);
    // Set the necessary command list state.
    graphicsCommandList->RSSetViewports(1, &m_viewport);
    graphicsCommandList->RSSetScissorRects(1, &m_scissorRect);
    graphicsCommandList->SetGraphicsRootSignature(m_gBufferPass.rootSignature.Get());
    ID3D12DescriptorHeap* texHeap = m_texPool.descriptorHeap();
    graphicsCommandList->SetDescriptorHeaps(1, &texHeap);
    // Finish the transition of the G-buffer to the writable state.
    D3D12_RESOURCE_BARRIER barriers[5];
    m_gBuffer.setWriteBarriers(barriers, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY);
    graphicsCommandList->ResourceBarrier(5, barriers);
    // Store columns 0, 1 and 3 of the view-projection matrix.
    const XMMATRIX tViewProj = XMMatrixTranspose(pCam.computeViewProjMatrix());
    XMFLOAT4A matCols[3];
    XMStoreFloat4A(&matCols[0], tViewProj.r[0]);
    XMStoreFloat4A(&matCols[1], tViewProj.r[1]);
    XMStoreFloat4A(&matCols[2], tViewProj.r[3]);
    // Set the root arguments.
    graphicsCommandList->SetGraphicsRoot32BitConstants(2, 12, matCols, 0);
    // Set the RTVs and the DSV.
    const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = {
        m_rtvPool.cpuHandle(BUF_CNT),    // First G-buffer RTV    
        m_rtvPool.cpuHandle(BUF_CNT + 3) // Material RTV
    };
    const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvPool.cpuHandle(0);
    graphicsCommandList->OMSetRenderTargets(4, &rtvHandles[0], true, &dsvHandle);
    // Only the material buffer needs to be cleared, the rest of the RTs can be discarded.
    graphicsCommandList->DiscardResource(m_gBuffer.normalBuffer.Get(),  nullptr);
    graphicsCommandList->DiscardResource(m_gBuffer.uvCoordBuffer.Get(), nullptr);
    graphicsCommandList->DiscardResource(m_gBuffer.uvGradBuffer.Get(),  nullptr);
    graphicsCommandList->ClearRenderTargetView(rtvHandles[1], FLOAT4_ZERO, 0, nullptr);
    // Clear the DSV.
    const D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;
    graphicsCommandList->ClearDepthStencilView(dsvHandle, clearFlags, 0, 0, 0, nullptr);
    // Define the input geometry.
    graphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    graphicsCommandList->IASetVertexBuffers(0, 3, scene.vertexAttrBuffers.views.get());
    // Compute the viewing frustum.
    Frustum frustum = pCam.computeViewFrustum();
    // Issue draw calls.
    uint16_t matId = UINT16_MAX;
    for (size_t i = 0, n = scene.objects.count; i < n; ++i) {
        // Test the object for visibility.
        if (frustum.intersects(scene.objects.boundingBoxes[i])) {
            if (matId != scene.objects.materialIndices[i]) {
                matId  = scene.objects.materialIndices[i];
                // Check whether the material has a valid bump map.
                uint32_t bumpMapFlag = 0;
                const uint32_t texId = scene.materials[matId].bumpTexId;
                if (texId < UINT32_MAX) {
                    bumpMapFlag = 1u << 31;
                    const D3D12_GPU_DESCRIPTOR_HANDLE texHandle = m_texPool.gpuHandle(texId);
                    graphicsCommandList->SetGraphicsRootDescriptorTable(0, texHandle);
                }
                // Set the bump map flag and the material index.
                graphicsCommandList->SetGraphicsRoot32BitConstant(1, bumpMapFlag | matId, 0);
            }
            // Set the index buffer.
            const D3D12_INDEX_BUFFER_VIEW ibv = scene.objects.indexBuffers.views[i];
            graphicsCommandList->IASetIndexBuffer(&scene.objects.indexBuffers.views[i]);
            // Draw the object.
            const uint32_t count = ibv.SizeInBytes / sizeof(uint32_t);
            graphicsCommandList->DrawIndexedInstanced(count, 1, 0, 0, 0);
        }
    }
}

void Renderer::recordShadingPass(const PerspectiveCamera& pCam) {
    ID3D12GraphicsCommandList* graphicsCommandList = m_graphicsContext.commandList(1);
    // Set the necessary command list state.
    graphicsCommandList->RSSetViewports(1, &m_viewport);
    graphicsCommandList->RSSetScissorRects(1, &m_scissorRect);
    graphicsCommandList->SetGraphicsRootSignature(m_shadingPass.rootSignature.Get());
    ID3D12DescriptorHeap* texHeap = m_texPool.descriptorHeap();
    graphicsCommandList->SetDescriptorHeaps(1, &texHeap);
    // Transition the G-buffer to the readable state.
    D3D12_RESOURCE_BARRIER barriers[6];
    m_gBuffer.setReadBarriers(barriers, D3D12_RESOURCE_BARRIER_FLAG_NONE);
    // Transition the state of the back buffer: Presenting -> Render Target.
    ID3D12Resource* backBuffer = m_swapChainBuffers[m_backBufferIndex].Get();
    barriers[5] = D3D12_TRANSITION_BARRIER{backBuffer, D3D12_RESOURCE_STATE_PRESENT,
                                                       D3D12_RESOURCE_STATE_RENDER_TARGET};
    graphicsCommandList->ResourceBarrier(6, barriers);
    // Store the 3x3 part of the raster-to-view-direction matrix.
    XMFLOAT3X3 rasterToViewDir;
    XMStoreFloat3x3(&rasterToViewDir, pCam.computeRasterToViewDirMatrix());
    // Set the root arguments.
    graphicsCommandList->SetGraphicsRoot32BitConstants(0, 9, &rasterToViewDir, 0);
    graphicsCommandList->SetGraphicsRootShaderResourceView(1, m_materialBuffer.view);
    // Set the SRVs of the G-buffer.
    graphicsCommandList->SetGraphicsRootDescriptorTable(2, m_texPool.gpuHandle(0));
    // Set the SRVs of all textures.
    graphicsCommandList->SetGraphicsRootDescriptorTable(3, m_texPool.gpuHandle(0));
    // Set the RTV.
    const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvPool.cpuHandle(m_backBufferIndex);
    graphicsCommandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    // The back buffer will be completely overwritten, so discarding it is sufficient.
    graphicsCommandList->DiscardResource(backBuffer, nullptr);
    // Perform the screen space pass using a single triangle.
    graphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    graphicsCommandList->DrawInstanced(3, 1, 0, 0);
    // Start the transition of the G-buffer to the writable state.
    m_gBuffer.setWriteBarriers(barriers, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY);
    // Transition the state of the back buffer: Render Target -> Presenting.
    barriers[5] = D3D12_TRANSITION_BARRIER{backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                       D3D12_RESOURCE_STATE_PRESENT};
    graphicsCommandList->ResourceBarrier(6, barriers);
}

void Renderer::renderFrame() {
    // Finalize and execute command lists.
    m_graphicsContext.executeCommandLists();
    // Present the frame, and update the index of the render (back) buffer.
    CHECK_CALL(m_swapChain->Present(VSYNC_INTERVAL, 0), "Failed to display the frame buffer.");
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    // Reset the graphics command (frame) allocator.
    m_graphicsContext.resetCommandAllocators();
    // Reset command lists to their initial states.
    m_graphicsContext.resetCommandList(0, m_gBufferPass.pipelineState.Get());
    m_graphicsContext.resetCommandList(1, m_shadingPass.pipelineState.Get());
    // Block the thread until the swap chain is ready accept a new frame.
    // Otherwise, Present() may block the thread, increasing the input lag.
    WaitForSingleObject(m_swapChainWaitableObject, INFINITE);
}

std::pair<uint64_t, uint64_t> Renderer::getTime() const {
    return m_graphicsContext.getTime();
}

void Renderer::stop() {
    m_copyContext.destroy();
    m_graphicsContext.destroy();
}
