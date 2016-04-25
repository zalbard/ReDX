#pragma once

#include <DirectXMathSSE4.h>
#include "HelperStructs.h"
#include "..\Common\Constants.h"
#include "..\Common\DynBitSet.h"
#include "..\Common\Scene.h"

namespace D3D12 {
    class Renderer {
    public:
        RULE_OF_ZERO_MOVE_ONLY(Renderer);
        Renderer();
        // Creates a 2D texture according to the provided description of the base MIP image.
        // Multi-sample textures and texture arrays are not supported.
        // Returns the texture and the index of the associated SRV within the texture pool.
        std::pair<Texture, uint> createTexture2D(const D3D12_SUBRESOURCE_FOOTPRINT& footprint,
                                                 const uint mipCount, const void* data);
        // Returns the index of the SRV within the texture pool.
        uint getTextureIndex(const Texture& texture) const;
        // Creates an index buffer for the index array with the specified number of indices.
        IndexBuffer createIndexBuffer(const uint count, const uint* indices);
        // Creates a vertex attribute buffer for the vertex array of 'count' elements.
        template <typename T>
        VertexBuffer createVertexBuffer(const uint count, const T* elements);
        // Sets materials (represented by texture indices) in shaders.
        void setMaterials(const uint count, const Material* materials);
        // Sets camera-related transformations in shaders.
        void setCameraTransforms(const PerspectiveCamera& pCam);
        // Submits all pending copy commands for execution, and begins a new segment
        // of the upload buffer. As a result, the previous segment of the buffer becomes
        // available for writing. 'immediateCopy' flag ensures that all copies from the
        // current segment are also completed during this call (at the cost of blocking
        // the thread), therefore making the entire buffer free and available for writing.
        void executeCopyCommands(const bool immediateCopy = false);
        // Records commands within the G-buffer generation pass.
        // Input: the camera and opaque scene objects.
        void recordGBufferPass(const PerspectiveCamera& pCam, const Scene::Objects& objects);
        // Records commands within the shading pass.
        void recordShadingPass();
        // Starts the frame rendering process.
        void renderFrame();
        // Returns the current time of the CPU thread and the GPU queue in microseconds.
        std::pair<uint64, uint64> getTime() const;
        // Terminates the rendering process.
        void stop();
    private:
        struct FrameResource {
            ConstantBuffer         transformBuffer;
            ComPtr<ID3D12Resource> depthBuffer; 
            ComPtr<ID3D12Resource> normalBuffer, uvCoordBuffer, uvGradBuffer, matIdBuffer;
            uint                   firstRtvIndex;
            uint                   firstTexIndex;
            void getTransitionBarriersToWritableState(D3D12_RESOURCE_BARRIER* barriers,
                                                      const D3D12_RESOURCE_BARRIER_FLAGS flag);
            void getTransitionBarriersToReadableState(D3D12_RESOURCE_BARRIER* barriers,
                                                      const D3D12_RESOURCE_BARRIER_FLAGS flag);
        };
        struct RenderPassConfig {
            ComPtr<ID3D12RootSignature> rootSignature;
            ComPtr<ID3D12PipelineState> pipelineState;
        };
        // Configures the G-buffer generation pass.
        void configureGBufferPass();
        // Configures the shading pass.
        void configureShadingPass();
        // Creates a depth buffer with descriptors in both DSV and texture pools.
        ComPtr<ID3D12Resource> createDepthBuffer(const uint width, const uint height,
                                                 const DXGI_FORMAT format);
        // Creates a render buffer with descriptors in both RTV and texture pools.
        ComPtr<ID3D12Resource> createRenderBuffer(const uint width, const uint height,
                                                  const DXGI_FORMAT format);
        // Creates a constant buffer for the data of the specified size (in bytes).
        ConstantBuffer createConstantBuffer(const uint size, const void* data = nullptr);
        // Creates a structured buffer for the data of the specified size (in bytes).
        StructuredBuffer createStructuredBuffer(const uint size, const void* data = nullptr);
        // Copies the data of the specified size (in bytes) and alignment into the upload buffer.
        // Returns the offset into the upload buffer which corresponds to the location of the data.
        template<uint alignment>
        uint copyToUploadBuffer(const uint size, const void* data);
        // Reserves a contiguous chunk of memory of the specified size within the upload buffer.
        // The reservation is guaranteed to be valid only until any other member function call.
        // Returns the address of and the offset to the beginning of the chunk of the upload buffer.
        template<uint alignment>
        std::pair<byte*, uint> reserveChunkOfUploadBuffer(const uint size);
    private:
        ComPtr<ID3D12DeviceEx>        m_device;
        D3D12_VIEWPORT                m_viewport;
        D3D12_RECT                    m_scissorRect;
        GraphicsContext<FRAME_CNT, 2> m_graphicsContext;
        uint                          m_frameIndex;
        uint                          m_backBufferIndex;
        ComPtr<ID3D12Resource>        m_renderTargets[BUF_CNT];
        FrameResource                 m_frameResouces[FRAME_CNT];
        RtvPool<RTV_CNT>              m_rtvPool;
        DsvPool<FRAME_CNT>            m_dsvPool;
        CbvSrvUavPool<TEX_CNT>        m_texPool;
        ComPtr<IDXGISwapChain3>       m_swapChain;
        HANDLE                        m_swapChainWaitableObject;
        CopyContext<2, 1>             m_copyContext;
        UploadRingBuffer              m_uploadBuffer;
        StructuredBuffer              m_materialBuffer;
        RenderPassConfig              m_gBufferPass;
        RenderPassConfig              m_shadingPass;
    };
} // namespace D3D12
