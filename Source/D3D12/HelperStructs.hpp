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

template <D3D12::QueueType T, uint N>
inline void D3D12::CommandQueue<T, N>::execute(ID3D12CommandList* const commandList) const {
    ID3D12CommandList* commandLists[] = {commandList};
    m_interface->ExecuteCommandLists(1, commandLists);
}

template <D3D12::QueueType T, uint N>
template<uint S>
inline void D3D12::CommandQueue<T, N>::execute(ID3D12CommandList* const (&commandLists)[S]) const {
    m_interface->ExecuteCommandLists(S, commandLists);
}

template<D3D12::QueueType T, uint N>
inline void D3D12::CommandQueue<T, N>::insertFence(const uint64 customFenceValue) {
    // Insert a fence into the command queue with a value appropriate for N-buffering
    // Once we reach the fence in the queue, a signal will go off,
    // which will set the internal value of the fence object to 'insertedValue'
    const uint64 insertedValue = customFenceValue ? customFenceValue
                                                  : N + m_fenceValue++;
    CHECK_CALL(m_interface->Signal(m_fence.Get(), insertedValue),
               "Failed to insert the fence into the command queue.");
}

template <D3D12::QueueType T, uint N>
inline void D3D12::CommandQueue<T, N>::blockThread(const uint64 customFenceValue) {
    // fence->GetCompletedValue() returns the value of the fence reached so far
    // If we haven't reached the fence yet...
    const uint64 awaitedValue = customFenceValue ? customFenceValue : m_fenceValue;
    if (m_fence->GetCompletedValue() < awaitedValue) {
        // ... we wait using a synchronization event
        CHECK_CALL(m_fence->SetEventOnCompletion(awaitedValue, m_syncEvent),
                   "Failed to set a synchronization event.");
        WaitForSingleObject(m_syncEvent, INFINITE);
    }
}

template <D3D12::QueueType T, uint N>
inline void D3D12::CommandQueue<T, N>::finish() {
    insertFence(UINT64_MAX);
    blockThread(UINT64_MAX);
    CloseHandle(m_syncEvent);
}

template<D3D12::QueueType T, uint N>
inline ID3D12CommandQueue* D3D12::CommandQueue<T, N>::get() {
    return m_interface.Get();
}

template<D3D12::QueueType T, uint N>
inline const ID3D12CommandQueue* D3D12::CommandQueue<T, N>::get() const {
    return m_interface.Get();
}

template<D3D12::QueueType T, uint N>
inline ID3D12CommandAllocator* D3D12::CommandQueue<T, N>::listAlloca() {
    return m_listAlloca[m_fenceValue % N].Get();
}

template<D3D12::QueueType T, uint N>
inline const ID3D12CommandAllocator* D3D12::CommandQueue<T, N>::listAlloca() const {
    return m_listAlloca[m_fenceValue % N].Get();
}

template<D3D12::QueueType T, uint N>
inline void D3D12::ID3D12DeviceEx::createCommandQueue(CommandQueue<T, N>* const commandQueue,
                                            const bool isHighPriority,
                                            const bool disableGpuTimeout) {
    assert(commandQueue);
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
    CHECK_CALL(CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue->m_interface)),
               "Failed to create a command queue.");
    // Create command allocators
    for (uint i = 0; i < N; ++i) {
        CHECK_CALL(CreateCommandAllocator(static_cast<D3D12_COMMAND_LIST_TYPE>(T),
                                          IID_PPV_ARGS(&commandQueue->m_listAlloca[i])),
                   "Failed to create a command list allocator.");
    }
    // Create a 0-initialized memory fence object
    commandQueue->m_fenceValue = 0;
    CHECK_CALL(CreateFence(commandQueue->m_fenceValue, D3D12_FENCE_FLAG_NONE,
                           IID_PPV_ARGS(&commandQueue->m_fence)),
               "Failed to create a memory fence object.");
    // Signal that we do not need to wait for the first N fences
    CHECK_CALL(commandQueue->m_interface->Signal(commandQueue->m_fence.Get(), N - 1),
               "Failed to insert the memory fence into the command queue.");
    // Create a synchronization event
    commandQueue->m_syncEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!commandQueue->m_syncEvent) {
        CHECK_CALL(HRESULT_FROM_WIN32(GetLastError()),
                   "Failed to create a synchronization event.");
    }
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
