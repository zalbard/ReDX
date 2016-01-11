#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl\client.h>
#include "..\Common\Definitions.h"

namespace D3D12 {
    using Microsoft::WRL::ComPtr;

    // Simple vertex representation
    struct Vertex {
        Vertex() = default;
        RULE_OF_ZERO(Vertex);
        DirectX::XMFLOAT3 position;  // Homogeneous screen-space coordinates from [-0.5, 0.5]
        DirectX::XMFLOAT4 color;     // RGBA color
    };

    // Direct3D vertex buffer
    struct VertexBuffer {
        VertexBuffer() = default;
        RULE_OF_ZERO(VertexBuffer);
        ComPtr<ID3D12Resource>   resource;  // Direct3D buffer interface
        D3D12_VERTEX_BUFFER_VIEW view;      // Buffer descriptor
    };

    // Direct3D index buffer
    struct IndexBuffer {
        IndexBuffer() = default;
        RULE_OF_ZERO(IndexBuffer);
        ComPtr<ID3D12Resource>  resource;  // Direct3D buffer interface
        D3D12_INDEX_BUFFER_VIEW view;      // Buffer descriptor
    };

    // Corresponds to Direct3D command list types
    enum class WorkType {
        GRAPHICS = D3D12_COMMAND_LIST_TYPE_DIRECT,  // Supports all types of commands
        COMPUTE  = D3D12_COMMAND_LIST_TYPE_COMPUTE, // Supports compute and copy commands only
        COPY     = D3D12_COMMAND_LIST_TYPE_COPY     // Supports copy commands only
    };

    // Direct3D command queue wrapper class
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
        friend struct ID3D12DeviceEx;
        ComPtr<ID3D12CommandQueue>     m_commandQueue;
        ComPtr<ID3D12CommandAllocator> m_listAlloca, m_bundleAlloca;
        /* Synchronization objects */
        ComPtr<ID3D12Fence>            m_fence;
        uint64                         m_fenceValue;
        HANDLE                         m_syncEvent;
    };

    // Corresponds to Direct3D descriptor types
    enum class DescType {
        // Constant Buffer Views | Shader Resource Views | Unordered Access Views
        CBV_SRV_UAV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 
        SAMPLER     = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     // Samplers
        RTV         = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,         // Render Target Views
        DSV         = D3D12_DESCRIPTOR_HEAP_TYPE_DSV          // Depth Stencil Views
    };

    // Direct3D descriptor heap
    template <DescType T>
    struct DescriptorHeap {
        DescriptorHeap() = default;
        RULE_OF_ZERO(DescriptorHeap);
        ComPtr<ID3D12DescriptorHeap> nativePtr;     // Ref-counted descriptor heap pointer
        D3D12_CPU_DESCRIPTOR_HANDLE  cpuBegin;      // CPU handle of the 1st descriptor of the heap
        D3D12_GPU_DESCRIPTOR_HANDLE  gpuBegin;      // GPU handle of the 1st descriptor of the heap
        uint                         handleIncrSz;  // Handle increment size
    };

    // ID3D12Device extension; uses the same UUID as ID3D12Device
    MIDL_INTERFACE("189819f1-1db6-4b57-be54-1821339b85f7")
    ID3D12DeviceEx: public ID3D12Device {
    public:
        ID3D12DeviceEx() = delete;
        RULE_OF_ZERO(ID3D12DeviceEx);
        // Creates a work queue of the specified type
        // Optionally, the work priority can be set to "high", and the GPU timeout can be disabled
        template <WorkType T>
        void createWorkQueue(WorkQueue<T>* const workQueue, 
                             const bool isHighPriority    = false, 
                             const bool disableGpuTimeout = false);
        // Creates a descriptor heap of the given type, size and shader visibility
        template <DescType T>
        void createDescriptorHeap(DescriptorHeap<T>* const descriptorHeap,
                                  const uint numDescriptors,
                                  const bool isShaderVisible = false);
        // Multi-GPU-adapter mask. Rendering is performed on a single GPU
        static constexpr uint nodeMask = 0;
    };
} // namespace D3D12
