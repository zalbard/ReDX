#pragma once

#include "HelperStructs.h"
#include "..\Common\Utility.h"

inline uint D3D12::VertexBuffer::count() const {
    return view.SizeInBytes / sizeof(Vertex);
}

inline uint D3D12::IndexBuffer::count() const {
    return view.SizeInBytes / sizeof(uint);
}

inline D3D12::UploadBuffer::UploadBuffer()
    : MemoryBuffer{nullptr}
    , begin{nullptr} {}

inline D3D12::UploadBuffer::UploadBuffer(UploadBuffer&& other) noexcept
    : MemoryBuffer{std::move(other.resource)}
    , begin{other.begin}
    , offset{other.offset}
    , capacity{other.capacity} {
    // Mark as moved
    other.begin = nullptr;
}

inline D3D12::UploadBuffer& D3D12::UploadBuffer::operator=(UploadBuffer&& other) noexcept {
    if (this != &other) {
        if (begin) {
            resource->Unmap(0, nullptr);
        }
        // Copy the data
        resource = std::move(other.resource);
        begin    = other.begin;
        offset   = other.offset;
        capacity = other.capacity;
        // Mark as moved
        other.begin = nullptr;
    }
    return *this;
}

inline D3D12::UploadBuffer::~UploadBuffer() noexcept {
    // Check if it was moved
    if (begin) {
        resource->Unmap(0, nullptr);
    }
}

template <D3D12::WorkType T>
inline D3D12::WorkQueue<T>::WorkQueue(WorkQueue&&) noexcept = default;

template <D3D12::WorkType T>
inline D3D12::WorkQueue<T>& D3D12::WorkQueue<T>::operator=(WorkQueue&&) noexcept = default;

template <D3D12::WorkType T>
inline D3D12::WorkQueue<T>::~WorkQueue() noexcept {
    // Check if it was moved
    if (m_fence) {
        waitForCompletion();
        CloseHandle(m_syncEvent);
    }
}

template<D3D12::WorkType T>
template<uint N>
inline void D3D12::WorkQueue<T>::execute(ID3D12CommandList* const (&commandLists)[N]) const {
    m_commandQueue->ExecuteCommandLists(N, commandLists);
}

template <D3D12::WorkType T>
inline void D3D12::WorkQueue<T>::waitForCompletion() {
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

template<D3D12::WorkType T>
inline ID3D12CommandQueue* D3D12::WorkQueue<T>::commandQueue() {
    return m_commandQueue.Get();
}

template<D3D12::WorkType T>
inline const ID3D12CommandQueue* D3D12::WorkQueue<T>::commandQueue() const {
    return m_commandQueue.Get();
}

template<D3D12::WorkType T>
inline ID3D12CommandAllocator* D3D12::WorkQueue<T>::listAlloca() {
    return m_listAlloca.Get();
}

template<D3D12::WorkType T>
inline const ID3D12CommandAllocator* D3D12::WorkQueue<T>::listAlloca() const {
    return m_listAlloca.Get();
}

template<D3D12::WorkType T>
inline ID3D12CommandAllocator* D3D12::WorkQueue<T>::bundleAlloca() {
    return m_bundleAlloca.Get();
}

template<D3D12::WorkType T>
inline const ID3D12CommandAllocator* D3D12::WorkQueue<T>::bundleAlloca() const {
    return m_bundleAlloca.Get();
}

template<D3D12::DescType T>
inline void D3D12::ID3D12DeviceEx::createDescriptorPool(DescriptorPool<T>* const descriptorPool,
                                                        const uint count,
                                                        const bool isShaderVisible) {
    assert(descriptorPool && count > 0);
    assert(T == DescType::CBV_SRV_UAV || T == DescType::SAMPLER || !isShaderVisible);
    constexpr auto nativeType = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(T);
    // Fill out the descriptor heap description
    const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
        /* Type */           nativeType,
        /* NumDescriptors */ count,
        /* Flags */          isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                             : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        /* NodeMask */       nodeMask
    };
    // Create a descriptor heap
    const auto device = static_cast<ID3D12Device*>(this);
    CHECK_CALL(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorPool->heap)),
               "Failed to create a descriptor heap.");
    // Get handles for the first descriptor of the heap
    descriptorPool->cpuBegin = descriptorPool->heap->GetCPUDescriptorHandleForHeapStart();
    descriptorPool->gpuBegin = descriptorPool->heap->GetGPUDescriptorHandleForHeapStart();
    // Get the increment size for descriptor handles
    descriptorPool->handleIncrSz = GetDescriptorHandleIncrementSize(nativeType);
}

template<D3D12::WorkType T>
inline void D3D12::ID3D12DeviceEx::createWorkQueue(WorkQueue<T>* const workQueue,
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
