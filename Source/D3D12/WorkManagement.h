#pragma once

#include <d3d12.h>
#include <wrl\client.h>
#include "..\Common\Definitions.h"

namespace D3D12 {
    using Microsoft::WRL::ComPtr;

    // Corresponds to Direct3D command list types
    enum class WorkType {
        GRAPHICS = D3D12_COMMAND_LIST_TYPE_DIRECT,  // Supports all types of commands
        COMPUTE  = D3D12_COMMAND_LIST_TYPE_COMPUTE, // Supports compute and copy commands only
        COPY     = D3D12_COMMAND_LIST_TYPE_COPY     // Supports copy commands only
    };

    // Direct3D command queue wrapper class
    template <WorkType T>
    class WorkQueue {
    public:
        WorkQueue() = default;
        RULE_OF_ZERO_MOVE_ONLY(WorkQueue);
        // Ctor; takes a Direct3D device and a command queue description as input
        explicit WorkQueue(ID3D12Device* const device, const D3D12_COMMAND_QUEUE_DESC& queueDesc);
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
        ComPtr<ID3D12CommandQueue>     m_commandQueue;
        ComPtr<ID3D12CommandAllocator> m_listAlloca, m_bundleAlloca;
        /* Synchronization objects */
        ComPtr<ID3D12Fence>            m_fence;
        uint64                         m_fenceValue;
        HANDLE                         m_syncEvent;
    };
} // namespace D3D12
