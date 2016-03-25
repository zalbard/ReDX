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

    template<CmdType T, uint N, uint L>
    inline auto CommandContext<T, N, L>::executeCommandList(const uint index)
    -> std::pair<ID3D12Fence*, uint64> {
        assert(index < L);
        // Close the command list.
        CHECK_CALL(m_commandLists[index]->Close(), "Failed to close the command list.");
        // Submit the command list for execution.
        ID3D12CommandList* const cmdList = m_commandLists[index].Get();
        m_commandQueue->ExecuteCommandLists(1, &cmdList);
        // Insert a fence (with the updated value) into the command queue.
        // Once we reach the fence in the queue, a signal will go off,
        // which will set the internal value of the fence object to 'value'.
        ID3D12Fence* const fence = m_fence.Get();
        const uint64       value = ++m_fenceValue;
        CHECK_CALL(m_commandQueue->Signal(fence, value),
                   "Failed to insert a fence into the command queue.");
        return {fence, value};
    }

    template<CmdType T, uint N, uint L>
    inline void CommandContext<T, N, L>::syncThread(const uint64 fenceValue) {
        // fence->GetCompletedValue() returns the value of the fence reached so far.
        // If we haven't reached the fence with 'fenceValue' yet...
        if (m_fence->GetCompletedValue() < fenceValue) {
            // ... we wait using a synchronization event.
            CHECK_CALL(m_fence->SetEventOnCompletion(fenceValue, m_syncEvent),
                       "Failed to set a synchronization event.");
            WaitForSingleObject(m_syncEvent, INFINITE);
        }
    }

    template<CmdType T, uint N, uint L>
    inline void CommandContext<T, N, L>::syncCommandQueue(ID3D12Fence* const fence,
                                                          const uint64 fenceValue) {
        CHECK_CALL(m_commandQueue->Wait(fence, fenceValue),
                   "Failed to start waiting for the fence.");
    }

    template<CmdType T, uint N, uint L>
    inline void CommandContext<T, N, L>::resetCommandAllocator() {
        // Update the value of the last inserted fence for the current allocator.
        m_lastFenceValues[m_allocatorIndex] = m_fenceValue;
        // Switch to the next command list allocator.
        const uint newAllocatorIndex = m_allocatorIndex = (m_allocatorIndex + 1) % N;
        // Command list allocators can only be reset when the associated 
        // command lists have finished execution on the GPU.
        // Therefore, we have to check the value of the fence, and wait if necessary.
        syncThread(m_lastFenceValues[newAllocatorIndex]);
        // It's now safe to reset the command allocator.
        CHECK_CALL(m_commandAllocators[newAllocatorIndex]->Reset(),
                   "Failed to reset the command list allocator.");
    }

    template<CmdType T, uint N, uint L>
    inline void CommandContext<T, N, L>::resetCommandList(const uint index,
                                                          ID3D12PipelineState* const state) {
        assert(index < L);
        // After a command list has been executed, it can be then
        // reset at any time (and must be before re-recording).
        CHECK_CALL(m_commandLists[index]->Reset(m_commandAllocators[m_allocatorIndex].Get(), state),
                   "Failed to reset the command list.");
    }

    template<CmdType T, uint N, uint L>
    inline void CommandContext<T, N, L>::destroy() {
        CHECK_CALL(m_commandQueue->Signal(m_fence.Get(), UINT64_MAX),
                   "Failed to insert a fence into the command queue.");
        syncThread(UINT64_MAX);
        CloseHandle(m_syncEvent);
    }

    template<CmdType T, uint N, uint L>
    inline auto CommandContext<T, N, L>::commandQueue()
    -> ID3D12CommandQueue* {
        return m_commandQueue.Get();
    }

    template<CmdType T, uint N, uint L>
    inline auto CommandContext<T, N, L>::commandQueue() const
    -> const ID3D12CommandQueue* {
        return m_commandQueue.Get();
    }

    template<CmdType T, uint N, uint L>
    inline auto CommandContext<T, N, L>::commandList(const uint index)
    -> ID3D12GraphicsCommandList* {
        assert(index < L);
        return m_commandLists[index].Get();
    }

    template<CmdType T, uint N, uint L>
    inline auto CommandContext<T, N, L>::commandList(const uint index) const
    -> const ID3D12GraphicsCommandList* {
        assert(index < L);
        return m_commandLists[index].Get();
    }

    template<CmdType T, uint N, uint L>
    inline void ID3D12DeviceEx::createCommandContext(CommandContext<T, N, L>* const commandContext,
                                                     const bool isHighPriority,
                                                     const bool disableGpuTimeout) {
        assert(commandContext);
        // Fill out the command queue description.
        const D3D12_COMMAND_QUEUE_DESC queueDesc = {
            /* Type */     static_cast<D3D12_COMMAND_LIST_TYPE>(T),
            /* Priority */ isHighPriority ? D3D12_COMMAND_QUEUE_PRIORITY_HIGH
                                          : D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            /* Flags */    disableGpuTimeout ? D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT
                                             : D3D12_COMMAND_QUEUE_FLAG_NONE,
            /* NodeMask */ nodeMask
        };
        // Create a command queue.
        CHECK_CALL(CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandContext->m_commandQueue)),
                   "Failed to create a command queue.");
        // Create command allocators.
        for (uint i = 0; i < N; ++i) {
            CHECK_CALL(CreateCommandAllocator(static_cast<D3D12_COMMAND_LIST_TYPE>(T),
                       IID_PPV_ARGS(&commandContext->m_commandAllocators[i])),
                       "Failed to create a command list allocator.");
        }
        // Set the initial allocator index to 0.
        commandContext->m_allocatorIndex = 0;
        // Create command lists in the closed NULL state using the initial allocator.
        for (uint i = 0; i < L; ++i) {
            CHECK_CALL(CreateCommandList(nodeMask, static_cast<D3D12_COMMAND_LIST_TYPE>(T),
                                         commandContext->m_commandAllocators[0].Get(), nullptr,
                                         IID_PPV_ARGS(&commandContext->m_commandLists[i])),
                       "Failed to create a command list.");
            CHECK_CALL(commandContext->m_commandLists[i]->Close(),
                       "Failed to close the command list.");

        }
        // Create a 0-initialized fence object.
        commandContext->m_fenceValue = 0;
        CHECK_CALL(CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&commandContext->m_fence)),
                   "Failed to create a fence object.");
        // Set the last fence value for each command allocator to 0.
        for (uint i = 0; i < N; ++i) {
            commandContext->m_lastFenceValues[i] = 0;
        }
        // Create a synchronization event.
        commandContext->m_syncEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!commandContext->m_syncEvent) {
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
