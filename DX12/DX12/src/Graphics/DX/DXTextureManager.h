#pragma once
#include "DXCommon.h"

#include "Texture/DXTexture.h"
#include "Utilities/HandlePool.h"

// DXTK for quick mip-mapped loading
#include "DXTK/WICTextureLoader.h"
#include "DXTK/ResourceUploadBatch.h"

	


// Strongly typed handle to a txture for the application to hold on to
struct TextureHandle
{
	TextureHandle() = default;

private:
	TextureHandle(uint64_t handle_) : handle(handle_) {}
	friend class DXTextureManager;
	uint64_t handle = 0;
};


struct DXTextureDesc
{
	UsageIntentCPU usage_cpu = UsageIntentCPU::eInvalid;
	UsageIntentGPU usage_gpu = UsageIntentGPU::eInvalid;
	TextureFlag flag = TextureFlag::eInvalid;


	// Init. data (we only support manual loads through files)
	std::filesystem::path filepath;
};

class DXTextureManager
{
public:
	DXTextureManager(cptr<ID3D12Device> dev, cptr<ID3D12CommandQueue> wait_queue);
	~DXTextureManager() = default;

	TextureHandle create_texture(const DXTextureDesc& desc);
	void destroy_texture(TextureHandle handle);

	ID3D12Resource* get_resource(TextureHandle tex);

	void create_srv(TextureHandle handle, D3D12_CPU_DESCRIPTOR_HANDLE descriptor);

private:
	friend class DXUploadContext;

	struct InternalTextureResource
	{
		DXTexture tex;

		// Metadata
		UsageIntentCPU usage_cpu = UsageIntentCPU::eInvalid;
		UsageIntentGPU usage_gpu = UsageIntentGPU::eInvalid;
		uint32_t frame_idx_allocation = 0;

		uint64_t handle = 0;
		void destroy() { tex.~tex(); }
	};

private:
	cptr<ID3D12Device> m_dev;
	cptr<ID3D12CommandQueue> m_wait_queue;

	HandlePool<InternalTextureResource> m_handles;
	

	uptr<DirectX::ResourceUploadBatch> m_up_batch;

};

