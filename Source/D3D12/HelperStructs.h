#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <utility>
#include <wrl\client.h>
#include "..\Common\Definitions.h"

namespace D3D12 {
    using DirectX::XMFLOAT3;
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
    enum class WorkType {
        GRAPHICS = D3D12_COMMAND_LIST_TYPE_DIRECT,  // Supports all types of commands
        COMPUTE  = D3D12_COMMAND_LIST_TYPE_COMPUTE, // Supports compute and copy commands only
        COPY     = D3D12_COMMAND_LIST_TYPE_COPY     // Supports copy commands only
    };

    struct Vertex {
        XMFLOAT3 position;                          // Object-space vertex coordinates
        XMFLOAT3 normal;                            // World-space normal vector
    };

    struct MemoryBuffer {
        ComPtr<ID3D12Resource>       resource;      // Buffer interface
    };

    struct VertexBuffer: public MemoryBuffer {
        D3D12_VERTEX_BUFFER_VIEW     view;          // Buffer descriptor
        uint                         count() const; // Returns the number of elements
    };

    struct IndexBuffer: public MemoryBuffer {
        D3D12_INDEX_BUFFER_VIEW      view;          // Buffer descriptor
        uint                         count() const; // Returns the number of elements
    };

    struct UploadBuffer: public MemoryBuffer {
        UploadBuffer();
        RULE_OF_FIVE_MOVE_ONLY(UploadBuffer);
        byte*                        begin;         // CPU virtual memory-mapped address
        uint                         offset;        // Offset from the beginning of the buffer
        uint                         capacity;      // Buffer size in bytes
    };

    // Descriptor heap wrapper
    template <DescType T>
    struct DescriptorPool {
        ComPtr<ID3D12DescriptorHeap> heap;          // Descriptor heap interface
        D3D12_CPU_DESCRIPTOR_HANDLE  cpuBegin;      // CPU handle of the 1st descriptor of the pool
        D3D12_GPU_DESCRIPTOR_HANDLE  gpuBegin;      // GPU handle of the 1st descriptor of the pool
        uint                         handleIncrSz;  // Handle increment size
    };

    // Command queue wrapper
    template <WorkType T>
    struct WorkQueue {
    public:
        WorkQueue() = default;
        RULE_OF_ZERO(WorkQueue);
        // Submits N command lists to the command queue for execution
        template <uint N>
        void execute(ID3D12CommandList* const (&commandLists)[N]) const;
        // Waits (using a fence) until the queue execution is finished
        void waitForCompletion();
        // Waits for all GPU commands to finish, and stops synchronization
        void finish();
        /* Accessors */
        ID3D12CommandQueue*            commandQueue();
        const ID3D12CommandQueue*      commandQueue() const;
        ID3D12CommandAllocator*        listAlloca();
        const ID3D12CommandAllocator*  listAlloca() const;
        ID3D12CommandAllocator*        bundleAlloca();
        const ID3D12CommandAllocator*  bundleAlloca() const;
    private:
        ComPtr<ID3D12CommandQueue>     m_commandQueue;  // Command queue interface
        ComPtr<ID3D12CommandAllocator> m_listAlloca,    // Bundle allocator interface
                                       m_bundleAlloca;  // Command list allocator interface
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
        ID3D12DeviceEx() = delete;
        RULE_OF_ZERO(ID3D12DeviceEx);
        // Creates a descriptor pool of the specified type, capacity and shader visibility
        template <DescType T>
        void createDescriptorPool(DescriptorPool<T>* const descriptorPool,
                                  const uint count, const bool isShaderVisible = false);
        // Creates a work queue of the specified type
        // Optionally, the work priority can be set to "high", and the GPU timeout can be disabled
        template <WorkType T>
        void createWorkQueue(WorkQueue<T>* const workQueue, 
                             const bool isHighPriority    = false, 
                             const bool disableGpuTimeout = false);
        // Multi-GPU-adapter mask. Rendering is performed on a single GPU
        static constexpr uint nodeMask = 0;
    };
} // namespace D3D12
