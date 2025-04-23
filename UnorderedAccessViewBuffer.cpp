#include "UnorderedAccessViewBuffer.h"

#include <DX12LibPCH.h>
#include <Application.h>
#include <ResourceStateTracker.h>

#include <d3dx12.h>

UnorderedAccessViewBuffer::UnorderedAccessViewBuffer(const std::wstring& name)
	: Buffer(name)
{
    // Allocate descriptors from linear-allocator for use within buffer
	m_UAV = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

UnorderedAccessViewBuffer::UnorderedAccessViewBuffer(const D3D12_RESOURCE_DESC& resourceDesc, size_t numElements, size_t elementSize, const std::wstring& name)
    : Buffer(resourceDesc, numElements, elementSize, name)
{
    m_UAV = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

// Copy of "Resource" initializer, for lazy initialization
void UnorderedAccessViewBuffer::SetResource(const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_CLEAR_VALUE* clearValue, const std::wstring& name)
{
    auto device = Application::Get().GetDevice();

    if (clearValue)
    {
        m_d3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*clearValue);
    }

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        m_d3d12ClearValue.get(),
        IID_PPV_ARGS(&m_d3d12Resource)
        ));

    ResourceStateTracker::AddGlobalResourceState(m_d3d12Resource.Get(), D3D12_RESOURCE_STATE_COMMON);

    SetName(name);
}

void UnorderedAccessViewBuffer::CreateViews(size_t ElementCount, size_t ElementSize)
{
    auto device = Application::Get().GetDevice();

    if (m_d3d12Resource)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.NumElements = static_cast<UINT>(ElementCount);
        uavDesc.Buffer.StructureByteStride = static_cast<UINT>(ElementSize);
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        device->CreateUnorderedAccessView(m_d3d12Resource.Get(),
            nullptr,
            &uavDesc,
            m_UAV.GetDescriptorHandle());
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE UnorderedAccessViewBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
    throw std::exception("UnorderedAccessViewBuffer::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE UnorderedAccessViewBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
    return m_UAV.GetDescriptorHandle();
}
