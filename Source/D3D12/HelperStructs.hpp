#pragma once

#include "HelperStructs.h"
#include "..\Common\Utility.h"

static inline uint width(const D3D12_RECT& rect) {
    return static_cast<uint>(rect.right - rect.left);
}

static inline uint height(const D3D12_RECT& rect) {
    return static_cast<uint>(rect.bottom - rect.top);
}

template<D3D12::WorkType T>
template<uint N>
void D3D12::WorkQueue<T>::execute(ID3D12CommandList* const (&commandLists)[N]) const {
    m_commandQueue->ExecuteCommandLists(N, commandLists);
}

template <D3D12::WorkType T>
void D3D12::WorkQueue<T>::waitForCompletion() {
    // Insert a memory fence (with a new value) into the GPU command queue
    // Once we reach the fence in the queue, a signal will go off,
    // which will set the internal value of the fence object to fenceValue
    CHECK_CALL(m_commandQueue->Signal(m_fence.Get(), ++m_fenceValue),
               "Failed to insert the memory fence into the command queue.");
    // fence->GetCompletedValue() returns the value of the fence reached so far
    // If we haven't reached the fence yet...
    if (m_fence->GetCompletedValue() < m_fenceValue) {
        // ... we wait using a synchronization event
        CHECK_CALL(m_fence->SetEventOnCompletion(m_fenceValue, m_syncEvent),
                   "Failed to set a synchronization event.");
        WaitForSingleObject(m_syncEvent, INFINITE);
    }
}

template <D3D12::WorkType T>
void D3D12::WorkQueue<T>::finish() {
    waitForCompletion();
    CloseHandle(m_syncEvent);
}

template<D3D12::WorkType T>
ID3D12CommandQueue* D3D12::WorkQueue<T>::commandQueue() {
    return m_commandQueue.Get();
}

template<D3D12::WorkType T>
const ID3D12CommandQueue* D3D12::WorkQueue<T>::commandQueue() const {
    return m_commandQueue.Get();
}

template<D3D12::WorkType T>
ID3D12CommandAllocator* D3D12::WorkQueue<T>::listAlloca() {
    return m_listAlloca.Get();
}

template<D3D12::WorkType T>
const ID3D12CommandAllocator* D3D12::WorkQueue<T>::listAlloca() const {
    return m_listAlloca.Get();
}

template<D3D12::WorkType T>
ID3D12CommandAllocator* D3D12::WorkQueue<T>::bundleAlloca() {
    return m_bundleAlloca.Get();
}

template<D3D12::WorkType T>
const ID3D12CommandAllocator* D3D12::WorkQueue<T>::bundleAlloca() const {
    return m_bundleAlloca.Get();
}

template<D3D12::WorkType T>
void D3D12::ID3D12DeviceEx::CreateWorkQueue(WorkQueue<T>* const workQueue,
                                            const bool isHighPriority,
                                            const bool disableGpuTimeout) {
    assert(workQueue);
    // Fill out the command queue description
    const D3D12_COMMAND_QUEUE_DESC queueDesc = {
        /* Type */     static_cast<D3D12_COMMAND_LIST_TYPE>(T),
        /* Priority */ isHighPriority ? D3D12_COMMAND_QUEUE_PRIORITY_HIGH
                                      : D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        /* Flags */    disableGpuTimeout ? D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT
                                         : D3D12_COMMAND_QUEUE_FLAG_NONE,
        /* NodeMask */ nodeMask
    };
    // Create a command queue
    CHECK_CALL(CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&workQueue->m_commandQueue)),
               "Failed to create a command queue.");
    // Create command allocators
    CHECK_CALL(CreateCommandAllocator(static_cast<D3D12_COMMAND_LIST_TYPE>(T),
                                      IID_PPV_ARGS(&workQueue->m_listAlloca)),
               "Failed to create a command list allocator.");
    CHECK_CALL(CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE,
                                      IID_PPV_ARGS(&workQueue->m_bundleAlloca)),
               "Failed to create a bundle command allocator.");
    // Create a 0-initialized memory fence object
    workQueue->m_fenceValue = 0;
    CHECK_CALL(CreateFence(workQueue->m_fenceValue, D3D12_FENCE_FLAG_NONE,
                           IID_PPV_ARGS(&workQueue->m_fence)),
               "Failed to create a memory fence object.");
    // Create a synchronization event
    workQueue->m_syncEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!workQueue->m_syncEvent) {
        CHECK_CALL(HRESULT_FROM_WIN32(GetLastError()),
                   "Failed to create a synchronization event.");
    }
}

template<D3D12::DescType T>
void D3D12::ID3D12DeviceEx::CreateDescriptorHeap(DescriptorHeap<T>* const descriptorHeap,
                                                 const uint numDescriptors,
                                                 const bool isShaderVisible) {
    assert(descriptorHeap);
    assert(T == DescType::CBV_SRV_UAV || T == DescType::SAMPLER || !isShaderVisible);
    constexpr auto nativeType = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(T);
    // Fill out the descriptor heap description
    const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
        /* Type */           nativeType,
        /* NumDescriptors */ numDescriptors,
        /* Flags */          isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                             : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        /* NodeMask */       nodeMask
    };
    // Create a descriptor heap
    const auto device = static_cast<ID3D12Device*>(this);
    CHECK_CALL(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap->nativePtr)),
               "Failed to create a descriptor heap.");
    // Get handles of the first descriptor of the heap
    descriptorHeap->cpuBegin = descriptorHeap->nativePtr->GetCPUDescriptorHandleForHeapStart();
    descriptorHeap->gpuBegin = descriptorHeap->nativePtr->GetGPUDescriptorHandleForHeapStart();
    // Get the increment size for descriptor handles
    descriptorHeap->handleIncrSz = GetDescriptorHandleIncrementSize(nativeType);
}
