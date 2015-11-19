#include "WorkManagement.h"
#include "..\Common\Utility.h"

using namespace D3D12;

WorkQueue::WorkQueue(ID3D12Device* const device, const uint nodeMask) {
    // Fill out the command queue description
    const D3D12_COMMAND_QUEUE_DESC queueDesc = {
        /* Type */     D3D12_COMMAND_LIST_TYPE_DIRECT,
        /* Priority */ D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        /* Flags */    D3D12_COMMAND_QUEUE_FLAG_NONE,
        /* NodeMask */ nodeMask
    };
    // Create a command queue
    CHECK_CALL(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue)),
               "Failed to create a command queue.");
    // Create a command allocator
    CHECK_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              IID_PPV_ARGS(&cmdAllocator)),
               "Failed to create a command allocator.");
    // Create a 0-initialized memory fence object
    CHECK_CALL(device->CreateFence(0ull, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)),
               "Failed to create a memory fence object.");
    // Set the first valid fence value to 1
    fenceValue = 1ull;
    // Create a synchronization event
    syncEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!syncEvent) {
        CHECK_CALL(HRESULT_FROM_WIN32(GetLastError()),
                   "Failed to create a synchronization event.");
    }
}

void WorkQueue::waitForCompletion() {
    // Insert the fence into the GPU command queue
    // Currently, fence->GetCompletedValue() < fenceValue (normally, off by 1)
    // Upon reaching the fence, a signal will increment the value of the fence
    CHECK_CALL(cmdQueue->Signal(fence.Get(), fenceValue),
               "Failed to insert the memory fence into the command queue.");
    // If we haven't reached the fence yet...
    if (fence->GetCompletedValue() < fenceValue) {
        // ... we wait using a synchronization event
        CHECK_CALL(fence->SetEventOnCompletion(fenceValue, syncEvent),
                   "Failed to set a synchronization event.");
        WaitForSingleObject(syncEvent, INFINITE);
    }
    // Increment the fence value s.t. it's once again off by 1
    ++fenceValue;
}

void WorkQueue::finish() {
    waitForCompletion();
    CloseHandle(syncEvent);
}
