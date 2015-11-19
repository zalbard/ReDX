#pragma once

#include <vector>
#include <d3d12.h>
#include <wrl\client.h>
#include "..\Common\Definitions.h"

namespace D3D12 {
    using Microsoft::WRL::ComPtr;

    // Direct3D command list wrapper class
    struct WorkList {
        RULE_OF_ZERO(WorkList);
        /* Public data members */
        ComPtr<ID3D12CommandList>      cmdList;
        ComPtr<ID3D12PipelineState>    pipelineState;
        ComPtr<ID3D12RootSignature>    rootSignature;
    };

    // Direct3D command queue wrapper class
    struct WorkQueue {
        RULE_OF_ZERO_MOVE_ONLY(WorkQueue);
        // Default constructor; does nothing
        WorkQueue() {}
        // Constructor; takes horizontal and vertical resolution as input
        WorkQueue(ID3D12Device* const device, const uint nodeMask);
        // Waits (using a fence) until the queue execution is finished
        void waitForCompletion();
        // Waits for all GPU commands to finish, and stops synchronization
        void finish();
        /* Public data members */
        ComPtr<ID3D12CommandQueue>     cmdQueue;
        ComPtr<ID3D12CommandAllocator> cmdAllocator;
        std::vector<WorkList>          workLists;
        /* Synchronization objects */
        HANDLE                         syncEvent;
        ComPtr<ID3D12Fence>            fence;
        uint64                         fenceValue;
    };
} // namespace D3D12
