#pragma once
#include "DXCommon.h"

#include "DXBufferManager.h"
#include "Profiler/GPUProfiler.h"

/*

	Work submission extension to BufferManager and TextureManager.
	This class accesses internals of BufferManager and TextureManager to do work.

*/
class DXUploadContext
{
public:
	DXUploadContext(cptr<ID3D12Device> dev, DXBufferManager* buf_mgr, uint32_t max_fif, GPUProfiler* profiler = nullptr);

	void frame_begin(uint32_t frame_idx);

	/*
		CopyBufferRegion
		CopyResource
		CopyTextureRegion
	
	*/

	// CPU-to-GPU buffer upload
	void upload_data(void* data, size_t size, BufferHandle hdl);




	// Dispatches all submitted work and signals at the end
	void submit_work(uint64_t sig_val);

	// The passed in queue will wait for the signal from the async copy
	void wait_for_async_copy(ID3D12CommandQueue* queue);


private:
	void update_constant(void* data, size_t size, DXBufferManager::InternalBufferResource* res);



private:
	cptr<ID3D12Device> m_dev;
	cptr<ID3D12CommandQueue> m_copy_queue;

	// paired list + allocator for each FIF
	std::vector<cptr<ID3D12GraphicsCommandList>> m_cmdls;
	std::vector<cptr<ID3D12CommandAllocator>> m_ators;

	// sync primitives 
	std::vector<DXFence> m_sync_prims;


	DXBufferManager* m_buf_mgr;
	// DXTextureManager* m_tex_mgr;

	uint32_t m_max_fif = 0;
	uint32_t m_curr_frame_idx = 0;

	GPUProfiler* m_profiler = nullptr;

};

