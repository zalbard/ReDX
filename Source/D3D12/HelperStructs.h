#pragma once

#include <d3d12.h>
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
    enum class CmdType {
        GRAPHICS = D3D12_COMMAND_LIST_TYPE_DIRECT,  // Supports all types of commands
        COMPUTE  = D3D12_COMMAND_LIST_TYPE_COMPUTE, // Supports compute and copy commands only
        COPY     = D3D12_COMMAND_LIST_TYPE_COPY     // Supports copy commands only
    };

    struct UploadRingBuffer {
        RULE_OF_FIVE_MOVE_ONLY(UploadRingBuffer);
        UploadRingBuffer();
        ComPtr<ID3D12Resource>       resource;      // Buffer interface
        byte*                        begin;         // CPU virtual memory-mapped address
        uint                         capacity;      // Buffer size in bytes
        uint                         offset;        // Offset from the beginning of the buffer
        uint                         prevSegStart;  // Offset to the beginning of the prev. segment
        uint                         currSegStart;  // Offset to the beginning of the curr. segment
    };

    struct VertexBuffer {
        ComPtr<ID3D12Resource>       resource;      // Buffer interface
        D3D12_VERTEX_BUFFER_VIEW     view;          // Buffer descriptor
    };

    struct IndexBuffer {
        ComPtr<ID3D12Resource>       resource;      // Buffer interface
        D3D12_INDEX_BUFFER_VIEW      view;          // Buffer descriptor
        uint                         count() const; // Returns the number of elements
    };

    struct ConstantBuffer {
        ComPtr<ID3D12Resource>       resource;      // Buffer interface
        D3D12_GPU_VIRTUAL_ADDRESS    location;      // GPU virtual address of the buffer
    };

    using D3D12_SHADER_RESOURCE_VIEW = D3D12_SHADER_RESOURCE_VIEW_DESC;

    struct ShaderResource {
        ComPtr<ID3D12Resource>       resource;      // Buffer interface
        D3D12_SHADER_RESOURCE_VIEW   view;          // Buffer descriptor
    };

    // Descriptor heap wrapper
    template <DescType T>
    struct DescriptorPool {
        ComPtr<ID3D12DescriptorHeap> heap;          // Descriptor heap interface
        D3D12_CPU_DESCRIPTOR_HANDLE  cpuBegin;      // CPU handle of the 1st descriptor of the pool
        D3D12_GPU_DESCRIPTOR_HANDLE  gpuBegin;      // GPU handle of the 1st descriptor of the pool
        uint                         handleIncrSz;  // Handle increment size
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
        void syncCommandQueue(ID3D12Fence* const fence, const uint64 fenceValue);
        // Resets the command list allocator. This function should be called once per frame.
        void resetCommandAllocator();
        // Resets the command list with the specified index to the specified state.
        // This function should be called after resetCommandAllocator().
        void resetCommandList(const uint index, ID3D12PipelineState* const state);
        // Waits for all command queue operations to complete, and stops synchronization.
        void destroy();
        /* Accessors */
        ID3D12CommandQueue*               commandQueue();
        const ID3D12CommandQueue*         commandQueue() const;
        ID3D12GraphicsCommandList*        commandList(const uint index);
        const ID3D12GraphicsCommandList*  commandList(const uint index) const;
    private:
        ComPtr<ID3D12CommandQueue>        m_commandQueue;
        uint                              m_allocatorIndex;
        ComPtr<ID3D12CommandAllocator>    m_commandAllocators[N];
        ComPtr<ID3D12GraphicsCommandList> m_commandLists[L];
        /* Synchronization objects */
        ComPtr<ID3D12Fence>               m_fence;
        uint64                            m_fenceValue;
        uint64                            m_lastFenceValues[N];
        HANDLE                            m_syncEvent;
        /* Accessors */
        friend struct ID3D12DeviceEx;
    };

    template <uint N, uint L> using GraphicsContext = CommandContext<CmdType::GRAPHICS, N, L>;
    template <uint N, uint L> using ComputeContext  = CommandContext<CmdType::COMPUTE,  N, L>;
    template <uint N, uint L> using CopyContext     = CommandContext<CmdType::COPY,     N, L>;

    // ID3D12Device extension; uses the same UUID as ID3D12Device
    MIDL_INTERFACE("189819f1-1db6-4b57-be54-1821339b85f7")
    ID3D12DeviceEx: public ID3D12Device {
    public:
        RULE_OF_ZERO(ID3D12DeviceEx);
        // Creates a command context of the specified type
        template <CmdType T, uint N, uint L>
        void createCommandContext(CommandContext<T, N, L>* const commandContext, 
                                  const bool isHighPriority    = false, 
                                  const bool disableGpuTimeout = false);

        // Creates a descriptor pool of the specified type and size (descriptor count)
        template <DescType T>
        void createDescriptorPool(DescriptorPool<T>* const descriptorPool, const uint count);
        // Creates a command queue of the specified type
        // Optionally, the queue priority can be set to 'high', and the GPU timeout can be disabled
        // Multi-GPU-adapter mask. Rendering is performed on a single GPU
        static constexpr uint nodeMask = 0;
    };
} // namespace D3D12
