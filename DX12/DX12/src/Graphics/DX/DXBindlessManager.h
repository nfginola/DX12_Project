#pragma once

#include "DXBindlessManager.h"
#include "DXBufferManager.h"
#include "DXTextureManager.h"
#include "Descriptor/DXDescriptorPool.h"
#include "Utilities/HandlePool.h"
#include "shaders/ShaderInterop_Renderer.h"

#include <unordered_map>

struct BindlessHandle
{
	BindlessHandle() = default;
private:
	BindlessHandle(uint64_t handle_) : handle(handle_) {}
	friend class DXBindlessManager;
	uint64_t handle = 0;
};


struct DXBindlessDesc
{
	TextureHandle diffuse_tex;
};

class DXBindlessManager
{
public:
	DXBindlessManager(
		Microsoft::WRL::ComPtr<ID3D12Device> dev,
		DXDescriptorAllocation&& bindless_part,		// descriptors managed by the bindless system
		DXBufferManager* buf_mgr,					// buffer creation interface to supply indices for each bindless primitive
		DXTextureManager* tex_mgr					// texture creation interface to access internals
	);

	void frame_begin(uint32_t frame_idx);

	BindlessHandle create_bindless(const DXBindlessDesc& desc);

	// Queues up the bindless chunk to the destruction queue (to be discarded after a full cycle from the requested frame
	// discard it for re-use
	void destroy_bindless(BindlessHandle handle);

	D3D12_GPU_DESCRIPTOR_HANDLE get_views_start() const;
	D3D12_GPU_DESCRIPTOR_HANDLE get_access_start() const;

	uint64_t offset_to_access_part() const;

	uint64_t access_index(BindlessHandle handle);

private:
	struct InternalBindlessResource
	{
		uint64_t access_index = 0;
		BufferHandle access_buf;

		DXDescriptorAllocation view_alloc, access_alloc;

		uint32_t frame_idx_allocation = 0;

		DXBindlessDesc desc;

		uint64_t handle = 0;
		void destroy() { }
	};

private:
	Microsoft::WRL::ComPtr<ID3D12Device> m_dev;
	DXBufferManager* m_buf_mgr = nullptr;
	DXTextureManager* m_tex_mgr = nullptr;
	std::unique_ptr<DXDescriptorPool> m_view_desc_ator;			// desc ator for views
	std::unique_ptr<DXDescriptorPool> m_access_desc_ator;		// desc ator for the access buffers (contiguous using CBV tables)
	HandlePool<InternalBindlessResource> m_handles;
	uint64_t m_offset_to_access_part = 0;

	uint64_t m_curr_max_indices = 0;
	std::queue<uint64_t> m_used_indices;

	uint32_t m_curr_frame_idx = 0;

	uint32_t m_max_elements = 0;

	std::queue<std::pair<uint32_t, std::function<void()>>> m_deletion_queue;

	D3D12_GPU_DESCRIPTOR_HANDLE m_views_start;
	D3D12_GPU_DESCRIPTOR_HANDLE m_access_start;

	uint32_t m_access_offset_from_base = 0;

	std::unordered_map<uint64_t, BindlessHandle> m_loaded_bindless;
};

