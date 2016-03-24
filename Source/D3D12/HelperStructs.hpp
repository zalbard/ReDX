#pragma once

#include <cassert>
#include "HelperStructs.h"
#include "..\Common\Utility.h"

namespace D3D12 {
    inline UploadRingBuffer::UploadRingBuffer()
        : resource{nullptr}
        , begin{nullptr}
        , capacity{0}
        , offset{0}
        , prevSegStart{UINT_MAX}
        , currSegStart{0} {}

    inline UploadRingBuffer::UploadRingBuffer(UploadRingBuffer&& other) noexcept
        : resource{std::move(other.resource)}
        , begin{other.begin}
        , capacity{other.capacity}
        , offset{other.offset}
        , prevSegStart{other.prevSegStart}
        , currSegStart{other.currSegStart} {
        // Mark as moved
        other.resource = nullptr;
    }

    inline UploadRingBuffer& UploadRingBuffer::operator=(UploadRingBuffer&& other) noexcept {
        if (resource) {
            resource->Unmap(0, nullptr);
        }
        // Copy the data
        resource     = std::move(other.resource);
        begin        = other.begin;
        capacity     = other.capacity;
        offset       = other.offset;
        prevSegStart = other.prevSegStart;
        currSegStart = other.currSegStart;
        // Mark as moved
        other.resource = nullptr;
        return *this;
    }

    inline UploadRingBuffer::~UploadRingBuffer() noexcept {
        // Check if it was moved
        if (resource) {
            resource->Unmap(0, nullptr);
        }
    }

    inline uint IndexBuffer::count() const {
        return view.SizeInBytes / sizeof(uint);
    }

    template <QueueType T, uint N>
    inline void
    CommandQueueEx<T, N>::execute(ID3D12CommandList* const commandList) const {
        assert(commandList);
        ID3D12CommandList* commandLists[] = {commandList};
        execute(commandLists);
    }

    template <QueueType T, uint N> template<uint K>
    inline void CommandQueueEx<T, N>::execute(ID3D12CommandList* const (&commandLists)[K]) const {
        m_interface->ExecuteCommandLists(K, commandLists);
    }

    template<QueueType T, uint N>
    inline auto CommandQueueEx<T, N>::insertFence(const uint64 customFenceValue)
    -> std::pair<ID3D12Fence*, uint64> {
        // Insert a fence into the command queue with a value appropriate for N-buffering
        // Once we reach the fence in the queue, a signal will go off,
        // which will set the internal value of the fence object to 'insertedValue'
        const uint64 insertedValue = customFenceValue ? customFenceValue
                                                      : N + m_fenceValue++;
        ID3D12Fence* const fence = m_fence.Get();
        CHECK_CALL(m_interface->Signal(fence, insertedValue),
                   "Failed to insert the fence into the command queue.");
        return {fence, insertedValue};
    }

    template <QueueType T, uint N>
    inline void CommandQueueEx<T, N>::syncThread(const uint64 customFenceValue) {
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

    template<QueueType T, uint N>
    inline void CommandQueueEx<T, N>::syncQueue(ID3D12Fence* const fence, const uint64 fenceValue) {
        CHECK_CALL(m_interface->Wait(fence, fenceValue), "Failed to start waiting for the fence.");
    }

    template <QueueType T, uint N>
    inline void CommandQueueEx<T, N>::finish() {
        insertFence(UINT64_MAX);
        syncThread(UINT64_MAX);
        CloseHandle(m_syncEvent);
    }

    template<QueueType T, uint N>
    inline ID3D12CommandQueue* CommandQueueEx<T, N>::get() {
        return m_interface.Get();
    }

    template<QueueType T, uint N>
    inline const ID3D12CommandQueue* CommandQueueEx<T, N>::get() const {
        return m_interface.Get();
    }

    template<QueueType T, uint N>
    inline ID3D12CommandAllocator* CommandQueueEx<T, N>::listAlloca() {
        return m_listAlloca[m_fenceValue % N].Get();
    }

    template<QueueType T, uint N>
    inline const ID3D12CommandAllocator* CommandQueueEx<T, N>::listAlloca() const {
        return m_listAlloca[m_fenceValue % N].Get();
    }

    template<QueueType T, uint N>
    inline void ID3D12DeviceEx::createCommandQueue(CommandQueueEx<T, N>* const commandQueue,
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
        // Create a 0-initialized fence object
        commandQueue->m_fenceValue = 0;
        CHECK_CALL(CreateFence(commandQueue->m_fenceValue, D3D12_FENCE_FLAG_NONE,
                               IID_PPV_ARGS(&commandQueue->m_fence)),
                   "Failed to create a fence object.");
        // Signal that we do not need to wait for the first N fences
        CHECK_CALL(commandQueue->m_interface->Signal(commandQueue->m_fence.Get(), N - 1),
                   "Failed to insert the fence into the command queue.");
        // Create a synchronization event
        commandQueue->m_syncEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!commandQueue->m_syncEvent) {
            CHECK_CALL(HRESULT_FROM_WIN32(GetLastError()),
                       "Failed to create a synchronization event.");
        }
    }

    template<DescType T>
    inline void ID3D12DeviceEx::createDescriptorPool(DescriptorPool<T>* const descriptorPool,
                                                     const uint count) {
        assert(descriptorPool && count > 0);
        constexpr auto nativeType      = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(T);
        constexpr bool isShaderVisible = DescType::CBV_SRV_UAV == T || DescType::SAMPLER == T;
        // Fill out the descriptor heap description
        const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
            /* Type */           nativeType,
            /* NumDescriptors */ count,
            /* Flags */          isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                                 : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            /* NodeMask */       nodeMask
        };
        // Create a descriptor heap
        CHECK_CALL(CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorPool->heap)),
                   "Failed to create a descriptor heap.");
        // Query and store the heap properties
        descriptorPool->cpuBegin     = descriptorPool->heap->GetCPUDescriptorHandleForHeapStart();
        descriptorPool->gpuBegin     = descriptorPool->heap->GetGPUDescriptorHandleForHeapStart();
        descriptorPool->handleIncrSz = GetDescriptorHandleIncrementSize(nativeType);
    }
} // namespace D3D12
