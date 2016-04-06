#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <memory>
#include <utility>
#include <wrl\client.h>
#include "..\Common\Constants.h"
#include "..\Common\Definitions.h"

namespace D3D12 {
    using Microsoft::WRL::ComPtr;

    struct CD3DX12_TEX2D_SRV_DESC: public D3D12_SHADER_RESOURCE_VIEW_DESC {
        CD3DX12_TEX2D_SRV_DESC() = default;
        explicit CD3DX12_TEX2D_SRV_DESC(const DXGI_FORMAT format, const uint mipCount,
            const uint  mostDetailedMip         = 0,
            const uint  planeSlice              = 0,
            const float resourceMinLODClamp     = 0.f,
            const uint  shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING);
    };

    struct UploadRingBuffer {
        RULE_OF_FIVE_MOVE_ONLY(UploadRingBuffer);
        UploadRingBuffer();
        ComPtr<ID3D12Resource>      resource;        // Memory buffer
        byte*                       begin;           // CPU virtual memory-mapped address
        uint                        capacity;        // Buffer size in bytes
        uint                        offset;          // Offset from the beginning of the buffer
        uint                        prevSegStart;    // Offset to the beginning of the prev. segment
        uint                        currSegStart;    // Offset to the beginning of the curr. segment
    };

    struct VertexBuffer {
        ComPtr<ID3D12Resource>      resource;        // Memory buffer
        D3D12_VERTEX_BUFFER_VIEW    view;            // Descriptor
    };

    struct IndexBuffer {
        ComPtr<ID3D12Resource>      resource;        // Memory buffer
        D3D12_INDEX_BUFFER_VIEW     view;            // Descriptor
        uint                        count() const;   // Returns the number of elements
    };

    struct ConstantBuffer {
        ComPtr<ID3D12Resource>      resource;        // Memory buffer
        D3D12_GPU_VIRTUAL_ADDRESS   view;            // Descriptor (Constant Buffer View)
    };

    struct Texture {
        ComPtr<ID3D12Resource>      resource;        // Memory buffer
        D3D12_CPU_DESCRIPTOR_HANDLE view;            // Descriptor handle (Shader Resource View)
    };

    // Stores objects of type T in the SoA layout.
    // T must be composed of 2 members: 'resource' and 'view'.
    template <typename T>
    struct ResourceViewSoA {
        // Allocates an SoA for 'count' elements.
        void allocate(const uint count);
        // Stores the object at the position denoted by 'index'.
        void assign(const uint index, T&& object);
        // Typedefs.
        using R = decltype(T::resource);
        using V = decltype(T::view);
    public:
        std::unique_ptr<R[]>        resources;       // Memory buffer array
        std::unique_ptr<V[]>        views;           // Descriptor [handle] array
    };

    using IndexBufferSoA = ResourceViewSoA<IndexBuffer>;

    // Corresponds to Direct3D descriptor types.
    enum class DescType {
        // Constant Buffer Views | Shader Resource Views | Unordered Access Views
        CBV_SRV_UAV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 
        SAMPLER     = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     // Samplers
        RTV         = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,         // Render Target Views
        DSV         = D3D12_DESCRIPTOR_HEAP_TYPE_DSV          // Depth Stencil Views
    };

    // Wrapper for a descriptor heap of capacity N.
    template <DescType T, uint N>
    struct DescriptorPool {
        // Returns the pointer to the underlying descriptor heap.
        ID3D12DescriptorHeap* descriptorHeap() const;
        // Returns the CPU handle of the descriptor stored at the 'index' position.
        D3D12_CPU_DESCRIPTOR_HANDLE       cpuHandle(const uint index);
        const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle(const uint index) const;
        // Returns the GPU handle of the descriptor stored at the 'index' position.
        D3D12_GPU_DESCRIPTOR_HANDLE       gpuHandle(const uint index);
        const D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle(const uint index) const;
    public:
        uint                         size     = 0;   // Current descriptor count
        static constexpr uint        capacity = N;   // Maximal descriptor count
    private:
        uint                         m_handleIncrSz; // Handle increment size
        D3D12_CPU_DESCRIPTOR_HANDLE  m_cpuBegin;     // CPU handle of the 1st descriptor of the pool
        D3D12_GPU_DESCRIPTOR_HANDLE  m_gpuBegin;     // GPU handle of the 1st descriptor of the pool
        ComPtr<ID3D12DescriptorHeap> m_heap;         // Descriptor heap interface
        /* Accessors */
        friend struct ID3D12DeviceEx;
    };

    template <uint N> using CbvSrvUavPool = DescriptorPool<DescType::CBV_SRV_UAV, N>;
    template <uint N> using SamplerPool   = DescriptorPool<DescType::SAMPLER, N>;
    template <uint N> using RtvPool       = DescriptorPool<DescType::RTV, N>;
    template <uint N> using DsvPool       = DescriptorPool<DescType::DSV, N>;

    // Corresponds to Direct3D command list types.
    enum class CmdType {
        GRAPHICS = D3D12_COMMAND_LIST_TYPE_DIRECT,   // Supports all types of commands
        COMPUTE  = D3D12_COMMAND_LIST_TYPE_COMPUTE,  // Supports compute and copy commands only
        COPY     = D3D12_COMMAND_LIST_TYPE_COPY      // Supports copy commands only
    };

    // Encapsulates an N-buffered command queue of type T with L command lists.
    template <CmdType T, uint N, uint L>
    struct CommandContext {
    public:
        RULE_OF_ZERO_MOVE_ONLY(CommandContext);
        CommandContext() = default;
        // Closes the command list with the specified index, submits it for execution,
        // and inserts a fence into the command queue afterwards.
        // Returns the inserted fence and its value.
        std::pair<ID3D12Fence*, uint64> executeCommandList(const uint index);
        // Stalls the execution of the current thread until
        // the fence with the specified value is reached.
        void syncThread(const uint64 fenceValue);
        // Stalls the execution of the command queue until
        // the fence with the specified value is reached.
        void syncCommandQueue(ID3D12Fence* fence, const uint64 fenceValue);
        // Resets the command list allocator, and returns the index of the new allocator.
        uint resetCommandAllocator();
        // Resets the command list with the specified index to the specified state.
        // Since it opens the command list, avoid calling it right before resetCommandAllocator().
        void resetCommandList(const uint index, ID3D12PipelineState* state);
        // Returns the current time of the CPU thread and the GPU queue in microseconds.
        std::pair<uint64, uint64> getTime() const;
        // Creates a swap chain for the window handle 'wHnd' according to the specified description.
        // Wraps around IDXGIFactory2::CreateSwapChainForHwnd().
        ComPtr<IDXGISwapChain3> createSwapChain(IDXGIFactory4* factory, const HWND hWnd,
                                                const DXGI_SWAP_CHAIN_DESC1& swapChainDesc);
        // Waits for all command queue operations to complete, and stops synchronization.
        void destroy();
        /* Accessors */
        ID3D12GraphicsCommandList*        commandList(const uint index);
        const ID3D12GraphicsCommandList*  commandList(const uint index) const;
    public:
        static constexpr uint             bufferCount      = N;
        static constexpr uint             commandListCount = L;
    private:
        ComPtr<ID3D12GraphicsCommandList> m_commandLists[L];
        ComPtr<ID3D12CommandQueue>        m_commandQueue;
        uint                              m_allocatorIndex;
        ComPtr<ID3D12CommandAllocator>    m_commandAllocators[N];
        /* Synchronization objects */
        uint64                            m_lastFenceValues[N];
        ComPtr<ID3D12Fence>               m_fence;
        uint64                            m_fenceValue;
        HANDLE                            m_syncEvent;
        /* Accessors */
        friend struct ID3D12DeviceEx;
    };

    template <uint N, uint L> using GraphicsContext = CommandContext<CmdType::GRAPHICS, N, L>;
    template <uint N, uint L> using ComputeContext  = CommandContext<CmdType::COMPUTE, N, L>;
    template <uint N, uint L> using CopyContext     = CommandContext<CmdType::COPY, N, L>;

    // ID3D12Device extension; uses the same UUID as ID3D12Device.
    MIDL_INTERFACE("189819f1-1db6-4b57-be54-1821339b85f7")
    ID3D12DeviceEx: public ID3D12Device {
    public:
        RULE_OF_ZERO(ID3D12DeviceEx);
        // Creates a command context of the specified type.
        // Optionally, the priority can be set to 'high', and the GPU timeout can be disabled.
        template <CmdType T, uint N, uint L>
        void createCommandContext(CommandContext<T, N, L>* commandContext, 
                                  const bool isHighPriority    = false, 
                                  const bool disableGpuTimeout = false);

        // Creates a descriptor pool of type T and size (descriptor count) N.
        template <DescType T, uint N>
        void createDescriptorPool(DescriptorPool<T, N>* descriptorPool);
        // Multi-GPU-adapter mask. Rendering is performed on a single GPU.
        static constexpr uint nodeMask = 0;
    };
} // namespace D3D12
