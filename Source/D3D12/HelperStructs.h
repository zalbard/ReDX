#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <utility>
#include <wrl\client.h>
#include "..\Common\Constants.h"
#include "..\Common\Definitions.h"

namespace D3D12 {
    using Microsoft::WRL::ComPtr;

    // Corresponds to Direct3D descriptor types
    enum class DescType {
        // Constant Buffer Views | Shader Resource Views | Unordered Access Views
        CBV_SRV_UAV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 
        SAMPLER     = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     // Samplers
        RTV         = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,         // Render Target Views
        DSV         = D3D12_DESCRIPTOR_HEAP_TYPE_DSV          // Depth Stencil Views
    };

    // Corresponds to Direct3D command list types
    enum class QueueType {
        GRAPHICS = D3D12_COMMAND_LIST_TYPE_DIRECT,  // Supports all types of commands
        COMPUTE  = D3D12_COMMAND_LIST_TYPE_COMPUTE, // Supports compute and copy commands only
        COPY     = D3D12_COMMAND_LIST_TYPE_COPY     // Supports copy commands only
    };

    struct MemoryBuffer {
        ComPtr<ID3D12Resource>       resource;      // Buffer interface
    protected:
        // Prevent polymorphic deletion
        ~MemoryBuffer() = default;
    };

    struct VertexBuffer: public MemoryBuffer {
        D3D12_VERTEX_BUFFER_VIEW     view;          // Buffer descriptor
    };

    struct IndexBuffer: public MemoryBuffer {
        D3D12_INDEX_BUFFER_VIEW      view;          // Buffer descriptor
        uint                         count() const; // Returns the number of elements
    };

    struct ConstantBuffer: public MemoryBuffer {
        D3D12_GPU_VIRTUAL_ADDRESS    location;      // GPU virtual address of the buffer
    };

    struct UploadRingBuffer: public MemoryBuffer {
        RULE_OF_FIVE_MOVE_ONLY(UploadRingBuffer);
        UploadRingBuffer();
        byte*                        begin;         // CPU virtual memory-mapped address
        uint                         capacity;      // Buffer size in bytes
        uint                         offset;        // Offset from the beginning of the buffer
        uint                         prevSegStart;  // Offset to the beginning of the prev. segment
        uint                         currSegStart;  // Offset to the beginning of the curr. segment
    };

    // Descriptor heap wrapper
    template <DescType T>
    struct DescriptorPool {
        ComPtr<ID3D12DescriptorHeap> heap;          // Descriptor heap interface
        D3D12_CPU_DESCRIPTOR_HANDLE  cpuBegin;      // CPU handle of the 1st descriptor of the pool
        D3D12_GPU_DESCRIPTOR_HANDLE  gpuBegin;      // GPU handle of the 1st descriptor of the pool
        uint                         handleIncrSz;  // Handle increment size
    };

    // Command queue wrapper with N allocators
    template <QueueType T, uint N>
    struct CommandQueue {
    public:
        RULE_OF_ZERO_MOVE_ONLY(CommandQueue);
        CommandQueue() = default;
        // Submits a single command list for execution
        void execute(ID3D12CommandList* const commandList) const;
        // Submits S command lists for execution
        template <uint S>
        void execute(ID3D12CommandList* const (&commandLists)[S]) const;
        // Inserts the fence into the queue
        // Optionally, a custom value of the fence can be specified
        // Returns the inserted fence and its value
        std::pair<ID3D12Fence*, uint64> insertFence(const uint64 customFenceValue = 0);
        // Blocks the execution of the thread until the fence is reached
        // Optionally, a custom value of the fence can be specified
        void syncThread(const uint64 customFenceValue = 0);
        // Blocks the execution of the queue until the fence
        // with the specified value is reached
        void syncQueue(ID3D12Fence* const fence, const uint64 fenceValue);
        // Waits for the queue to become drained, and stops synchronization
        void finish();
        /* Accessors */
        ID3D12CommandQueue*            get();
        const ID3D12CommandQueue*      get() const;
        ID3D12CommandAllocator*        listAlloca();
        const ID3D12CommandAllocator*  listAlloca() const;
    private:
        ComPtr<ID3D12CommandQueue>     m_interface;     // Command queue interface
        ComPtr<ID3D12CommandAllocator> m_listAlloca[N]; // Command list allocator interface
        /* Synchronization objects */
        ComPtr<ID3D12Fence>            m_fence;
        uint64                         m_fenceValue;
        HANDLE                         m_syncEvent;
        /* Accessors */
        friend struct ID3D12DeviceEx;
    };

    // ID3D12Device extension; uses the same UUID as ID3D12Device
    MIDL_INTERFACE("189819f1-1db6-4b57-be54-1821339b85f7")
    ID3D12DeviceEx: public ID3D12Device {
    public:
        RULE_OF_ZERO(ID3D12DeviceEx);
        template<QueueType T, uint N>
        void createCommandQueue(CommandQueue<T, N>* const commandQueue, 
                                const bool isHighPriority    = false, 
                                const bool disableGpuTimeout = false);
        // Creates a descriptor pool of the specified type, capacity and shader visibility
        template <DescType T>
        void createDescriptorPool(DescriptorPool<T>* const descriptorPool,
                                  const uint count, const bool isShaderVisible = false);
        // Creates a command queue of the specified type
        // Optionally, the queue priority can be set to 'high', and the GPU timeout can be disabled
        // Multi-GPU-adapter mask. Rendering is performed on a single GPU
        static constexpr uint nodeMask = 0;
    };
} // namespace D3D12
