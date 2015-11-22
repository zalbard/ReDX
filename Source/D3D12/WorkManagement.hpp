#include "WorkManagement.h"
#include "..\Common\Utility.h"

namespace D3D12 {
    template <WorkType T>
    WorkQueue<T>::WorkQueue(ID3D12Device* const device, const uint nodeMask) {
        // Fill out the command queue description
        const D3D12_COMMAND_QUEUE_DESC queueDesc = {
            /* Type */     static_cast<D3D12_COMMAND_LIST_TYPE>(T),
            /* Priority */ D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            /* Flags */    D3D12_COMMAND_QUEUE_FLAG_NONE,
            /* NodeMask */ nodeMask
        };
        // Create a command queue
        CHECK_CALL(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_cmdQueue)),
                   "Failed to create a command queue.");
        // Create command allocators
        CHECK_CALL(device->CreateCommandAllocator(static_cast<D3D12_COMMAND_LIST_TYPE>(T),
                                                  IID_PPV_ARGS(&m_listAlloca)),
                   "Failed to create a command list allocator.");
        CHECK_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE,
                                                  IID_PPV_ARGS(&m_bundleAlloca)),
                   "Failed to create a bundle command allocator.");
        // Create a 0-initialized memory fence object
        m_fenceValue = 0ull;
        CHECK_CALL(device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)),
                   "Failed to create a memory fence object.");
        // Create a synchronization event
        m_syncEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!m_syncEvent) {
            CHECK_CALL(HRESULT_FROM_WIN32(GetLastError()),
                       "Failed to create a synchronization event.");
        }
    }

    template<WorkType T>
    template<uint N>
    void WorkQueue<T>::executeCmdLists(ID3D12CommandList* const (&cmdLists)[N]) {
        m_cmdQueue->ExecuteCommandLists(N, cmdLists);
    }

    template <WorkType T>
    void WorkQueue<T>::waitForCompletion() {
        // Insert a memory fence (with a new value) into the GPU command queue
        // Once we reach the fence in the queue, a signal will go off,
        // which will set the internal value of the fence object to fenceValue
        CHECK_CALL(m_cmdQueue->Signal(m_fence.Get(), ++m_fenceValue),
                   "Failed to insert the memory fence into the command queue.");
        // fence->GetCompletedValue() retuns the value of the fence reached so far
        // If we haven't reached the fence yet...
        if (m_fence->GetCompletedValue() < m_fenceValue) {
            // ... we wait using a synchronization event
            CHECK_CALL(m_fence->SetEventOnCompletion(m_fenceValue, m_syncEvent),
                       "Failed to set a synchronization event.");
            WaitForSingleObject(m_syncEvent, INFINITE);
        }
    }

    template <WorkType T>
    void WorkQueue<T>::finish() {
        waitForCompletion();
        CloseHandle(m_syncEvent);
    }

    template<WorkType T>
    ID3D12CommandQueue* WorkQueue<T>::cmdQueue() {
        return m_cmdQueue.Get();
    }
} // namespace D3D12
