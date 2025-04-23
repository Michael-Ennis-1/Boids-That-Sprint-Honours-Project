#pragma once
#include <Buffer.h>
#include <DescriptorAllocation.h>

class UnorderedAccessViewBuffer : public Buffer
{
public:
	// Initialize buffer with default resource view
	UnorderedAccessViewBuffer(const std::wstring& name = L"");

	// Initialze buffer with custom resource view
	UnorderedAccessViewBuffer(const D3D12_RESOURCE_DESC& resourceDesc, size_t numElements, size_t elementSize, const std::wstring& name);

	// Initialze resource given a resource description
	void SetResource(const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_CLEAR_VALUE* clearValue, const std::wstring& name);

	// Initialze GPU-Visible descriptors
	virtual void CreateViews(size_t ElementCount, size_t ElementSize) override;

	// This is not an SRV, as such this should never be called
    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override;

	// Get CPU-Visible descriptor handle for Unordered Access View
    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override;

	// Store CPU-Visible descriptor handle
	DescriptorAllocation m_UAV;
};
