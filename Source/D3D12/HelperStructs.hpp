#pragma once

#include <cassert>
#include "HelperStructs.h"
#include "..\Common\Utility.h"

namespace D3D12 {
    static inline auto getTypelessFormat(const DXGI_FORMAT dsvFormat)
    -> DXGI_FORMAT {
        DXGI_FORMAT format;
        switch (dsvFormat) {
        case DXGI_FORMAT_D16_UNORM:
            format = DXGI_FORMAT_R16_TYPELESS;
            break;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            format = DXGI_FORMAT_R24G8_TYPELESS;
            break;
        case DXGI_FORMAT_D32_FLOAT:
            format = DXGI_FORMAT_R32_TYPELESS;
            break;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            format = DXGI_FORMAT_R32G8X24_TYPELESS;
            break;
        default:
            printError("The format doesn't contain a depth component.");
            TERMINATE();
        }
        return format;
    }

    static inline auto getDepthSrvFormat(const DXGI_FORMAT dsvFormat)
    -> DXGI_FORMAT {
        DXGI_FORMAT format;
        switch (dsvFormat) {
        case DXGI_FORMAT_D16_UNORM:
            format = DXGI_FORMAT_R16G16_UNORM;
            break;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            break;
        case DXGI_FORMAT_D32_FLOAT:
            format = DXGI_FORMAT_R32_FLOAT;
            break;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
            break;
        default:
            printError("The format doesn't contain a depth component.");
            TERMINATE();
        }
        return format;
    }

    static inline auto getStencilSrvFormat(const DXGI_FORMAT dsvFormat)
    -> DXGI_FORMAT {
        DXGI_FORMAT format;
        switch (dsvFormat) {
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
            break;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
            break;
        default:
            printError("The format doesn't contain a stencil component.");
            TERMINATE();
        }
        return format;
    }

    inline D3D12_TEX2D_SRV_DESC::D3D12_TEX2D_SRV_DESC(const DXGI_FORMAT format,
                                                      const uint32_t mipCount,
                                                      const uint32_t mostDetailedMip,
                                                      const uint32_t planeSlice,
                                                      const float    resourceMinLODClamp,
                                                      const uint32_t shader4ComponentMapping) {
        Format                        = format;
        ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
        Shader4ComponentMapping       = shader4ComponentMapping;
        Texture2D.MipLevels           = mipCount;
        Texture2D.MostDetailedMip     = mostDetailedMip;
        Texture2D.PlaneSlice          = planeSlice;
        Texture2D.ResourceMinLODClamp = resourceMinLODClamp;
    }

    inline D3D12_TRANSITION_BARRIER::D3D12_TRANSITION_BARRIER(ID3D12Resource* resource,
                                     const D3D12_RESOURCE_STATES before,
                                     const D3D12_RESOURCE_STATES after,
                                     const D3D12_RESOURCE_BARRIER_FLAGS flag)
        : D3D12_RESOURCE_BARRIER{
            /* Type */        D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            /* Flags */       flag,
            /* pResource */   resource,
            /* Subresource */ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            /* StateBefore */ before,
            /* StateAfter */  after
        }
    {}

    inline auto D3D12_TRANSITION_BARRIER::Begin(ID3D12Resource* resource,
                                                const D3D12_RESOURCE_STATES before,
                                                const D3D12_RESOURCE_STATES after)
    -> D3D12_TRANSITION_BARRIER {
        return D3D12_TRANSITION_BARRIER{resource, before, after,
                                        D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY};
    }

    inline auto D3D12_TRANSITION_BARRIER::End(ID3D12Resource* resource,
                                              const D3D12_RESOURCE_STATES before,
                                              const D3D12_RESOURCE_STATES after)
    -> D3D12_TRANSITION_BARRIER {
        return D3D12_TRANSITION_BARRIER{resource, before, after,
                                        D3D12_RESOURCE_BARRIER_FLAG_END_ONLY};
    }

    inline UploadRingBuffer::UploadRingBuffer()
        : resource{nullptr}
        , begin{nullptr}
        , capacity{0}
        , offset{0}
        , prevSegStart{0}
        , currSegStart{0} {}

    inline auto UploadRingBuffer::remainingCapacity() const
    -> size_t {
        const sign_t dist = prevSegStart - offset;
        // Account for the wrap-around.
        return (prevSegStart <= offset) ? capacity + dist : dist;
    }

    inline auto UploadRingBuffer::previousSegmentSize() const
    -> size_t {
        const sign_t dist = currSegStart - prevSegStart;
        // Account for the wrap-around.
        return (prevSegStart <= currSegStart) ? dist : capacity + dist;
    }

    inline UploadRingBuffer::UploadRingBuffer(UploadRingBuffer&& other) noexcept
        : resource{std::move(other.resource)}
        , begin{other.begin}
        , capacity{other.capacity}
        , offset{other.offset}
        , prevSegStart{other.prevSegStart}
        , currSegStart{other.currSegStart} {
        // Mark as moved.
        other.resource = nullptr;
    }

    inline UploadRingBuffer& UploadRingBuffer::operator=(UploadRingBuffer&& other) noexcept {
        if (resource) {
            resource->Unmap(0, nullptr);
        }
        // Copy the data.
        resource     = std::move(other.resource);
        begin        = other.begin;
        capacity     = other.capacity;
        offset       = other.offset;
        prevSegStart = other.prevSegStart;
        currSegStart = other.currSegStart;
        // Mark as moved.
        other.resource = nullptr;
        return *this;
    }

    inline UploadRingBuffer::~UploadRingBuffer() noexcept {
        // Check if it was moved.
        if (resource) {
            resource->Unmap(0, nullptr);
        }
    }

    template<typename T>
    inline void ResourceViewSoA<T>::allocate(const size_t count) {
        assert(!resources && !views);
        resources = std::make_unique<Resource[]>(count);
        views     = std::make_unique<View[]>(count);
    }

    template<typename T>
    inline void ResourceViewSoA<T>::assign(const size_t index, const T& object) {
        resources[index] = object.resource;
        views[index]     = object.view;
    }

    template<typename T>
    inline void ResourceViewSoA<T>::assign(const size_t index, T&& object) {
        resources[index] = std::move(object.resource);
        views[index]     = std::move(object.view);
    }

    template<DescType T, size_t N>
    inline auto DescriptorPool<T, N>::descriptorHeap()
    -> ID3D12DescriptorHeap* {
        return m_heap.Get();
    }

    template<DescType T, size_t N>
    inline auto DescriptorPool<T, N>::cpuHandle(const size_t index)
    -> D3D12_CPU_DESCRIPTOR_HANDLE {
        assert(index < capacity);
        return D3D12_CPU_DESCRIPTOR_HANDLE{index * m_handleIncrSz + m_cpuBegin.ptr};
    }

    template<DescType T, size_t N>
    inline auto DescriptorPool<T, N>::gpuHandle(const size_t index)
    -> D3D12_GPU_DESCRIPTOR_HANDLE {
        assert(index < capacity);
        return D3D12_GPU_DESCRIPTOR_HANDLE{index * m_handleIncrSz + m_gpuBegin.ptr};
    }

    template<DescType T, size_t N>
    inline auto DescriptorPool<T, N>::computeIndex(const D3D12_CPU_DESCRIPTOR_HANDLE handle) const
    -> size_t {
        return (handle.ptr - m_cpuBegin.ptr) / m_handleIncrSz;
    }

    template<DescType T, size_t N>
    inline auto DescriptorPool<T, N>::computeIndex(const D3D12_GPU_DESCRIPTOR_HANDLE handle) const
    -> size_t {
        return (handle.ptr - m_gpuBegin.ptr) / m_handleIncrSz;
    }

    template<CmdType T, size_t N, size_t L>
    inline auto CommandContext<T, N, L>::executeCommandList(const size_t index)
    -> std::pair<ID3D12Fence*, uint64_t> {
        assert(index < L);
        // Close the command list.
        CHECK_CALL(m_commandLists[index]->Close(), "Failed to close the command list.");
        // Submit the command list for execution.
        ID3D12CommandList* cmdList = m_commandLists[index].Get();
        m_commandQueue->ExecuteCommandLists(1, &cmdList);
        // Insert a fence (with the updated value) into the command queue.
        // Once we reach the fence in the queue, a signal will go off,
        // which will set the internal value of the fence object to 'value'.
        ID3D12Fence*   fence = m_fence.Get();
        const uint64_t value = ++m_fenceValue;
        CHECK_CALL(m_commandQueue->Signal(fence, value),
                   "Failed to insert a fence into the command queue.");
        return {fence, value};
    }

    template<CmdType T, size_t N, size_t L>
    inline auto CommandContext<T, N, L>::executeCommandLists()
    -> std::pair<ID3D12Fence*, uint64_t> {
        // Close command lists.
        ID3D12CommandList* commandLists[L];
        for (size_t i = 0; i < L; ++i) {
            commandLists[i] = m_commandLists[i].Get();
            CHECK_CALL(m_commandLists[i]->Close(), "Failed to close the command list.");
        }
        // Submit command lists for execution.
        m_commandQueue->ExecuteCommandLists(L, commandLists);
        // Insert a fence (with the updated value) into the command queue.
        // Once we reach the fence in the queue, a signal will go off,
        // which will set the internal value of the fence object to 'value'.
        ID3D12Fence*   fence = m_fence.Get();
        const uint64_t value = ++m_fenceValue;
        CHECK_CALL(m_commandQueue->Signal(fence, value),
                   "Failed to insert a fence into the command queue.");
        return {fence, value};
    }

    template<CmdType T, size_t N, size_t L>
    inline void CommandContext<T, N, L>::syncThread(const uint64_t fenceValue) {
        // fence->GetCompletedValue() returns the value of the fence reached so far.
        // If we haven't reached the fence with 'fenceValue' yet...
        if (m_fence->GetCompletedValue() < fenceValue) {
            // ... we wait using a synchronization event.
            CHECK_CALL(m_fence->SetEventOnCompletion(fenceValue, m_syncEvent),
                       "Failed to set a synchronization event.");
            WaitForSingleObject(m_syncEvent, INFINITE);
        }
    }

    template<CmdType T, size_t N, size_t L>
    inline void CommandContext<T, N, L>::syncCommandQueue(ID3D12Fence* fence,
                                                          const uint64_t fenceValue) {
        CHECK_CALL(m_commandQueue->Wait(fence, fenceValue),
                   "Failed to start waiting for the fence.");
    }

    template<CmdType T, size_t N, size_t L>
    inline void CommandContext<T, N, L>::resetCommandAllocators() {
        // Update the value of the last inserted fence for the current allocator set.
        m_lastFenceValues[m_frameAllocatorSet] = m_fenceValue;
        // Switch to the allocator set for the next frame.
        const size_t nextAllocatorSet = m_frameAllocatorSet = (m_frameAllocatorSet + 1) % N;
        // Command list allocators can only be reset when the associated 
        // command lists have finished execution on the GPU.
        // Therefore, we have to check the value of the fence, and wait if necessary.
        syncThread(m_lastFenceValues[nextAllocatorSet]);
        // It's now safe to reset the command allocators.
        for (size_t i = 0; i < L; ++i) {
            CHECK_CALL(m_commandAllocators[nextAllocatorSet][i]->Reset(),
                       "Failed to reset the command list allocator.");
        }
    }

    template<CmdType T, size_t N, size_t L>
    inline void CommandContext<T, N, L>::resetCommandList(const size_t index,
                                                          ID3D12PipelineState* state) {
        assert(index < L);
        // After a command list has been executed, it can be then
        // reset at any time (and must be before re-recording).
        auto commandListAllocator = m_commandAllocators[m_frameAllocatorSet][index].Get();
        CHECK_CALL(m_commandLists[index]->Reset(commandListAllocator, state),
                   "Failed to reset the command list.");
    }

    template<CmdType T, size_t N, size_t L>
    inline auto CommandContext<T, N, L>::getTime() const
    -> std::pair<uint64_t, uint64_t> {
        // Query the frequencies (ticks/second).
        uint64_t cpuFrequency, gpuFrequency;
        QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&cpuFrequency));
        m_commandQueue->GetTimestampFrequency(&gpuFrequency);
        // Sample the time stamp counters.
        uint64_t cpuTimeStamp, gpuTimeStamp;
        m_commandQueue->GetClockCalibration(&gpuTimeStamp, &cpuTimeStamp);
        // Use the frequencies to perform conversions to microseconds.
        const uint64_t cpuTime = (cpuTimeStamp * 1000000) / cpuFrequency;
        const uint64_t gpuTime = (gpuTimeStamp * 1000000) / gpuFrequency;
        return {cpuTime, gpuTime};
    }

    template<CmdType T, size_t N, size_t L>
    inline auto CommandContext<T, N, L>::createSwapChain(IDXGIFactory4* factory, const HWND hWnd,
                                                         const DXGI_SWAP_CHAIN_DESC1& swapChainDesc)
    -> ComPtr<IDXGISwapChain3> {
        ComPtr<IDXGISwapChain3> swapChain;
        auto swapChainAddr = reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf());
        CHECK_CALL(factory->CreateSwapChainForHwnd(m_commandQueue.Get(), hWnd, &swapChainDesc, 
                                                   nullptr, nullptr, swapChainAddr),
                   "Failed to create a swap chain.");
        return swapChain;
    }

    template<CmdType T, size_t N, size_t L>
    inline void CommandContext<T, N, L>::destroy() {
        CHECK_CALL(m_commandQueue->Signal(m_fence.Get(), UINT64_MAX),
                   "Failed to insert a fence into the command queue.");
        syncThread(UINT64_MAX);
        if (!CloseHandle(m_syncEvent)) {
            printError("Failed to close the synchronization event handle."); 
            TERMINATE();
        }
        for (size_t i = 0; i < L; ++i) {
            // Command lists have to be released before the associated
            // root signatures and pipeline state objects.
            m_commandLists[i].Reset();
        }
    }

    template<CmdType T, size_t N, size_t L>
    inline auto CommandContext<T, N, L>::commandList(const size_t index)
    -> ID3D12GraphicsCommandList* {
        assert(index < L);
        return m_commandLists[index].Get();
    }

    template<CmdType T, size_t N, size_t L>
    inline auto CommandContext<T, N, L>::commandList(const size_t index) const
    -> const ID3D12GraphicsCommandList* {
        assert(index < L);
        return m_commandLists[index].Get();
    }

    template<CmdType T, size_t N, size_t L>
    inline void ID3D12DeviceEx::createCommandContext(CommandContext<T, N, L>* commandContext,
                                                     const bool isHighPriority,
                                                     const bool disableGpuTimeout) {
        static_assert(N > 0 && L > 0, "Invalid command context parameters.");
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
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < L; ++j) {
                CHECK_CALL(CreateCommandAllocator(static_cast<D3D12_COMMAND_LIST_TYPE>(T),
                           IID_PPV_ARGS(&commandContext->m_commandAllocators[i][j])),
                           "Failed to create a command list allocator.");
            }
        }
        // Set the initial frame allocator set index to 0.
        commandContext->m_frameAllocatorSet = 0;
        // Create command lists in the closed, NULL state using the initial allocator.
        for (size_t i = 0; i < L; ++i) {
            CHECK_CALL(CreateCommandList(nodeMask, static_cast<D3D12_COMMAND_LIST_TYPE>(T),
                                         commandContext->m_commandAllocators[0][i].Get(), nullptr,
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
        for (size_t i = 0; i < N; ++i) {
            commandContext->m_lastFenceValues[i] = 0;
        }
        // Create a synchronization event.
        commandContext->m_syncEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!commandContext->m_syncEvent) {
            CHECK_CALL(HRESULT_FROM_WIN32(GetLastError()),
                       "Failed to create a synchronization event.");
        }
    }

    template<DescType T, size_t N>
    inline void ID3D12DeviceEx::createDescriptorPool(DescriptorPool<T, N>* descriptorPool) {
        static_assert(N > 0, "Invalid descriptor pool capacity.");
        assert(descriptorPool);
        constexpr auto heapType        = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(T);
        constexpr bool isShaderVisible = DescType::CBV_SRV_UAV == T || DescType::SAMPLER == T;
        // Fill out the descriptor heap description.
        const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
            /* Type */           heapType,
            /* NumDescriptors */ N,
            /* Flags */          isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                                 : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            /* NodeMask */       nodeMask
        };
        // Create a descriptor heap.
        CHECK_CALL(CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorPool->m_heap)),
                   "Failed to create a descriptor heap.");
        // Query and store the heap properties.
        descriptorPool->size = 0;
        descriptorPool->m_cpuBegin = descriptorPool->m_heap->GetCPUDescriptorHandleForHeapStart();
        descriptorPool->m_gpuBegin = descriptorPool->m_heap->GetGPUDescriptorHandleForHeapStart();
        descriptorPool->m_handleIncrSz = GetDescriptorHandleIncrementSize(heapType);
    }
} // namespace D3D12
