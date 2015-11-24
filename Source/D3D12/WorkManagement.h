#pragma once

#include <d3d12.h>
#include <wrl\client.h>
#include "..\Common\Definitions.h"

namespace D3D12 {
    using Microsoft::WRL::ComPtr;

    // Corresponds to Direct3D command list types
    enum class WorkType { GRAPHICS = D3D12_COMMAND_LIST_TYPE_DIRECT,
                          COMPUTE  = D3D12_COMMAND_LIST_TYPE_COMPUTE };

    // Direct3D command list wrapper class
    template <WorkType T>
    struct WorkList {
        ComPtr<ID3D12CommandList>   cmdList;
        ComPtr<ID3D12PipelineState> pipelineState;
        ComPtr<ID3D12RootSignature> rootSignature;
    };

    // Direct3D command queue wrapper class
    template <WorkType T>
    class WorkQueue {
    public:
        WorkQueue() = default;
        RULE_OF_ZERO_MOVE_ONLY(WorkQueue);
        // Initializer; takes a Direct3D device and a device (node) mask as input
        void init(ID3D12Device* const device, const uint nodeMask);
        // Submits the command lists to the command queue for execution
        template <uint N>
        void executeCmdLists(ID3D12CommandList* const (&cmdLists)[N]);
        // Waits (using a fence) until the queue execution is finished
        void waitForCompletion();
        // Waits for all GPU commands to finish, and stops synchronization
        void finish();
        // Returns the pointer to the command queue
        ID3D12CommandQueue* cmdQueue();
        // Returns the pointer to the command list allocator
        ID3D12CommandAllocator* listAlloca();
        // Returns the pointer to the bundle allocator
        ID3D12CommandAllocator* bundleAlloca();
    private:
        ComPtr<ID3D12CommandQueue>     m_cmdQueue;
        ComPtr<ID3D12CommandAllocator> m_listAlloca, m_bundleAlloca;
        /* Synchronization objects */
        ComPtr<ID3D12Fence>            m_fence;
        uint64                         m_fenceValue;
        HANDLE                         m_syncEvent;
    };

    using GraphicsWorkQueue = WorkQueue<WorkType::GRAPHICS>;
    using ComputeWorkQueue  = WorkQueue<WorkType::COMPUTE>;
} // namespace D3D12
