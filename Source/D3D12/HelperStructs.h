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

    struct D3D12_TEX2D_SRV_DESC: public D3D12_SHADER_RESOURCE_VIEW_DESC {
        explicit D3D12_TEX2D_SRV_DESC(const DXGI_FORMAT format, const size_t mipCount,
                                      const size_t   mostDetailedMip         = 0,
                                      const size_t   planeSlice              = 0,
                                      const float    resourceMinLODClamp     = 0.f,
                                      const uint32_t shader4ComponentMapping =
                                      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING);
    };

    struct D3D12_TRANSITION_BARRIER: public D3D12_RESOURCE_BARRIER {
        explicit D3D12_TRANSITION_BARRIER(ID3D12Resource* resource,
                                          const D3D12_RESOURCE_STATES before,
                                          const D3D12_RESOURCE_STATES after,
                                          const D3D12_RESOURCE_BARRIER_FLAGS flag =
                                          D3D12_RESOURCE_BARRIER_FLAG_NONE);
        // Performs the state transition with the 'BEGIN_ONLY' flag.
        static D3D12_TRANSITION_BARRIER Begin(ID3D12Resource* resource,
                                              const D3D12_RESOURCE_STATES before,
                                              const D3D12_RESOURCE_STATES after);
        // Performs the state transition with the 'END_ONLY' flag.
        static D3D12_TRANSITION_BARRIER End(ID3D12Resource* resource,
                                            const D3D12_RESOURCE_STATES before,
                                            const D3D12_RESOURCE_STATES after);
    };

    struct UploadRingBuffer {
        RULE_OF_FIVE_MOVE_ONLY(UploadRingBuffer);
        UploadRingBuffer();
        // Returns the amount of unused space (in bytes) in the buffer.
        // Effectively, computes the distance from 'offset' to 'prevSegStart'
        // (or 'currSegStart' if the former is invalid) with respect to the wrap-around.
        size_t remainingCapacity() const;
        // Returns the size (in bytes) of the previous segment of the buffer.
        size_t previousSegmentSize() const;
    public:
        ComPtr<ID3D12Resource>      resource;        // Memory buffer
        byte_t*                     begin;           // CPU virtual memory-mapped address
        uint32_t                    capacity;        // Buffer size (in bytes)
        uint32_t                    offset;          // Offset from the beginning of the buffer
        uint32_t                    prevSegStart;    // Offset to the beginning of the prev. segment
        uint32_t                    currSegStart;    // Offset to the beginning of the curr. segment
    };

    struct VertexBuffer {
        ComPtr<ID3D12Resource>      resource;        // Memory buffer
        D3D12_VERTEX_BUFFER_VIEW    view;            // Descriptor
    };

    struct IndexBuffer {
        ComPtr<ID3D12Resource>      resource;        // Memory buffer
        D3D12_INDEX_BUFFER_VIEW     view;            // Descriptor
    };

    // Ideally suited for uniform (convergent) access patterns.
    struct ConstantBuffer {
        ComPtr<ID3D12Resource>      resource;        // Memory buffer
        D3D12_GPU_VIRTUAL_ADDRESS   view;            // Descriptor (Constant Buffer View)
    };

    // Ideally suited for non-uniform (divergent) access patterns.
    struct StructuredBuffer {
        ComPtr<ID3D12Resource>      resource;        // Memory buffer
        D3D12_GPU_VIRTUAL_ADDRESS   view;            // Descriptor (Shader Resource View)
    };

    struct Texture {
        ComPtr<ID3D12Resource>      resource;        // Memory buffer
        D3D12_GPU_DESCRIPTOR_HANDLE view;            // Descriptor handle (Shader Resource View)
    };

    // Stores objects of type T in the SoA layout.
    // T must be composed of 2 members: 'resource' and 'view'.
    template <typename T>
    struct ResourceViewSoA {
        // Allocates an SoA for 'count' elements.
        void allocate(const size_t count);
        // Copies the object to the position denoted by 'index'.
        void assign(const size_t index, const T& object);
        // Moves the object to the position denoted by 'index'.
        void assign(const size_t index, T&& object);
        // Typedefs.
        using Resource = decltype(T::resource);
        using View     = decltype(T::view);
    public:
        std::unique_ptr<Resource[]> resources;       // Memory buffer array
        std::unique_ptr<View[]>     views;           // Descriptor [handle] array
    };

    using VertexBufferSoA     = ResourceViewSoA<VertexBuffer>;
    using IndexBufferSoA      = ResourceViewSoA<IndexBuffer>;
    using ConstantBufferSoA   = ResourceViewSoA<ConstantBuffer>;
    using StructuredBufferSoA = ResourceViewSoA<StructuredBuffer>;
    using TextureSoA          = ResourceViewSoA<Texture>;

    // Corresponds to Direct3D descriptor types.
    enum class DescType {
        // Constant Buffer Views | Shader Resource Views | Unordered Access Views
        CBV_SRV_UAV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        SAMPLER     = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     // Samplers
        RTV         = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,         // Render Target Views
        DSV         = D3D12_DESCRIPTOR_HEAP_TYPE_DSV          // Depth Stencil Views
    };

    // Wrapper for a descriptor heap of capacity N.
    template <DescType T, size_t N>
    struct DescriptorPool {
        // Returns the pointer to the underlying descriptor heap.
        ID3D12DescriptorHeap* descriptorHeap();
        // Returns the CPU handle of the descriptor stored at the 'index' position.
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle(const size_t index);
        // Returns the GPU handle of the descriptor stored at the 'index' position.
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle(const size_t index);
        // Computes the position (offset in the number of descriptors) of the descriptor handle.
        size_t computeIndex(const D3D12_CPU_DESCRIPTOR_HANDLE handle) const;
        size_t computeIndex(const D3D12_GPU_DESCRIPTOR_HANDLE handle) const;
    public:
        static constexpr size_t      capacity = N;   // Maximal descriptor count
        uint32_t                     size;           // Current descriptor count
    private:
        uint32_t                     m_handleIncrSz; // Descriptor handle increment size
        D3D12_CPU_DESCRIPTOR_HANDLE  m_cpuBegin;     // CPU handle of the 1st descriptor of the pool
        D3D12_GPU_DESCRIPTOR_HANDLE  m_gpuBegin;     // GPU handle of the 1st descriptor of the pool
        ComPtr<ID3D12DescriptorHeap> m_heap;         // Descriptor heap interface
        /* Accessors */
        friend struct ID3D12DeviceEx;
    };

    template <size_t N> using CbvSrvUavPool = DescriptorPool<DescType::CBV_SRV_UAV, N>;
    template <size_t N> using SamplerPool   = DescriptorPool<DescType::SAMPLER, N>;
    template <size_t N> using RtvPool       = DescriptorPool<DescType::RTV, N>;
    template <size_t N> using DsvPool       = DescriptorPool<DescType::DSV, N>;

    // Corresponds to Direct3D command list types.
    enum class CmdType {
        GRAPHICS = D3D12_COMMAND_LIST_TYPE_DIRECT,   // Supports all types of commands
        COMPUTE  = D3D12_COMMAND_LIST_TYPE_COMPUTE,  // Supports compute and copy commands only
        COPY     = D3D12_COMMAND_LIST_TYPE_COPY      // Supports copy commands only
    };

    // Encapsulates an N-buffered command queue of type T with L command lists.
    template <CmdType T, size_t N, size_t L>
    struct CommandContext {
    public:
        RULE_OF_ZERO_MOVE_ONLY(CommandContext);
        CommandContext() = default;
        // Closes the command list with the specified index, submits it for execution,
        // and inserts a fence into the command queue afterwards.
        // Returns the inserted fence and its value.
        std::pair<ID3D12Fence*, uint64_t> executeCommandList(const size_t index);
        // Closes all command lists, submits them for execution in ascending order,
        // and inserts a fence into the command queue afterwards.
        // Returns the inserted fence and its value.
        std::pair<ID3D12Fence*, uint64_t> executeCommandLists();
        // Stalls the execution of the current thread until
        // the fence with the specified value is reached.
        void syncThread(const uint64_t fenceValue);
        // Stalls the execution of the command queue until
        // the fence with the specified value is reached.
        void syncCommandQueue(ID3D12Fence* fence, const uint64_t fenceValue);
        // Resets the set of command list allocators for the current frame.
        void resetCommandAllocators();
        // Resets the command list with the specified index to the specified state.
        // Since it opens the command list, avoid calling it right before resetCommandAllocators().
        void resetCommandList(const size_t index, ID3D12PipelineState* state);
        // Returns the current time of the CPU thread and the GPU queue in microseconds.
        std::pair<uint64_t, uint64_t> getTime() const;
        // Creates a swap chain for the window handle 'wHnd' according to the specified description.
        // The swap chain needs the command queue in order to be able to flush the latter.
        // Wraps around IDXGIFactory2::CreateSwapChainForHwnd().
        ComPtr<IDXGISwapChain3> createSwapChain(IDXGIFactory4* factory, const HWND hWnd,
                                                const DXGI_SWAP_CHAIN_DESC1& swapChainDesc);
        // Waits for all command queue operations to complete, and stops synchronization.
        void destroy();
        /* Accessors */
        ID3D12GraphicsCommandList*        commandList(const size_t index);
        const ID3D12GraphicsCommandList*  commandList(const size_t index) const;
    public:
        static constexpr size_t           bufferCount      = N;
        static constexpr size_t           commandListCount = L;
    private:
        ComPtr<ID3D12GraphicsCommandList> m_commandLists[L];
        ComPtr<ID3D12CommandQueue>        m_commandQueue;
        size_t                            m_frameAllocatorSet;
        ComPtr<ID3D12CommandAllocator>    m_commandAllocators[N][L];
        /* Synchronization objects */
        uint64_t                          m_lastFenceValues[N];
        ComPtr<ID3D12Fence>               m_fence;
        uint64_t                          m_fenceValue;
        HANDLE                            m_syncEvent;
        /* Accessors */
        friend struct ID3D12DeviceEx;
    };

    template <size_t N, size_t L> using GraphicsContext = CommandContext<CmdType::GRAPHICS, N, L>;
    template <size_t N, size_t L> using ComputeContext  = CommandContext<CmdType::COMPUTE, N, L>;
    template <size_t N, size_t L> using CopyContext     = CommandContext<CmdType::COPY, N, L>;

    // ID3D12Device extension; uses the same UUID as ID3D12Device.
    MIDL_INTERFACE("189819f1-1db6-4b57-be54-1821339b85f7")
    ID3D12DeviceEx: public ID3D12Device {
    public:
        RULE_OF_ZERO(ID3D12DeviceEx);
        // Creates a command context of the specified type.
        // Optionally, the priority can be set to 'high', and the GPU timeout can be disabled.
        template <CmdType T, size_t N, size_t L>
        void createCommandContext(CommandContext<T, N, L>* commandContext, 
                                  const bool isHighPriority    = false, 
                                  const bool disableGpuTimeout = false);
        // Creates a descriptor pool of type T and size (descriptor count) N.
        template <DescType T, size_t N>
        void createDescriptorPool(DescriptorPool<T, N>* descriptorPool);
        // Multi-GPU-adapter mask. Rendering is performed on a single GPU.
        static constexpr uint32_t nodeMask = 0;
    };
} // namespace D3D12
