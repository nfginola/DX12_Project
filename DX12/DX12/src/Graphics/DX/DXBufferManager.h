#pragma once
#include <functional>
#include "Utilities/HandlePool.h"
#include "Graphics/DX/DXCommon.h"


#include "Buffer/DXBufferPoolAllocator.h"			// Suballocators
#include "Buffer/DXBufferRingPoolAllocator.h"		// Suballocators
#include "Buffer/DXBufferGenericAllocator.h"		// Committed resource allocator

// Allocation algorithms for constant data management
class DXConstantRingBuffer;
class DXConstantStaticBuffer;

// Strongly typed handle to a buffer for the application to hold on to
struct BufferHandle
{
public:
	BufferHandle() = default;
	bool valid() const { return handle != 0; }
private:
	BufferHandle(uint64_t handle_) : handle(handle_) {}
	friend class DXBufferManager;
	uint64_t handle = 0;
};


struct DXBufferDesc
{
	UsageIntentCPU usage_cpu = UsageIntentCPU::eInvalid;
	UsageIntentGPU usage_gpu = UsageIntentGPU::eInvalid;
	BufferFlag flag = BufferFlag::eInvalid;
	bool is_rt_structure = false;

	uint32_t element_size = 0;
	uint32_t element_count = 0;

	// Init. data (optional)
	void* data = nullptr;
	size_t data_size = 0;
};

class DXBufferManager
{

public:
	DXBufferManager(Microsoft::WRL::ComPtr<ID3D12Device> dev, uint32_t max_fif);
	~DXBufferManager();

	void frame_begin(uint32_t frame_idx);

	BufferHandle create_buffer(const DXBufferDesc& desc);
	void destroy_buffer(BufferHandle hdl);
	 
	// Maybe should be moved to a DXRootArgBinder which has a reference to a BufferManager and a TextureManager?
	void bind_as_direct_arg(ID3D12GraphicsCommandList* cmdl, BufferHandle buf, UINT param_idx, RootArgDest dest, bool write = false);

	void create_cbv(BufferHandle handle, D3D12_CPU_DESCRIPTOR_HANDLE descriptor);	// constant view
	void create_srv(BufferHandle handle, D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t start_el, uint32_t num_el, bool raw = false);		// structured view
	void create_rt_accel_view(BufferHandle handle, D3D12_CPU_DESCRIPTOR_HANDLE descriptor);
	D3D12_INDEX_BUFFER_VIEW get_ibv(BufferHandle handle, DXGI_FORMAT format = DXGI_FORMAT_R32_UINT);

	uint32_t get_element_count(BufferHandle handle);

	const DXBufferAllocation* get_buffer_alloc(BufferHandle handle);

private:
	friend class DXUploadContext;

	struct InternalBufferResource
	{
		DXBufferAllocation alloc;

		// Metadata
		bool is_transient = false;
		bool is_constant = false;
		UsageIntentCPU usage_cpu = UsageIntentCPU::eInvalid;
		UsageIntentGPU usage_gpu = UsageIntentGPU::eInvalid;
		uint64_t total_requested_size = 0;
		uint32_t frame_idx_allocation = 0;
		bool is_rt_structure = false;

		uint64_t handle = 0;
		void destroy() { alloc.~alloc(); }
	};

	InternalBufferResource* get_internal_buf(BufferHandle handle);


private:

	InternalBufferResource* create_constant(const DXBufferDesc& desc);
	InternalBufferResource* create_non_constant(const DXBufferDesc& desc);
	void destroy_constant(InternalBufferResource* res);

private:
	Microsoft::WRL::ComPtr<ID3D12Device> m_dev;
	HandlePool<InternalBufferResource> m_handles;
	uint32_t m_curr_frame_idx = 0;

	bool m_first_frame = true;



	std::unique_ptr<DXBufferRingPoolAllocator> m_constant_ring_buf;


	// one for each FIF since we resource states can be different..
	/*
		
	"At any given time, a subresource is in exactly one state, determined by the set of D3D12_RESOURCE_STATES flags supplied to ResourceBarrier. 
	The application must ensure that the before and after states of consecutive calls to ResourceBarrier agree."	
	https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12
	
	*/
	// one for each FIF since we resource states can be different..
	std::vector<std::unique_ptr<DXBufferPoolAllocator>> m_constant_persistent_bufs;
	//std::unique_ptr<DXBufferPoolAllocator> m_constant_persistent_buf;


	std::queue<std::pair<uint32_t, std::function<void()>>> m_deletion_queue;		// pair: [frame idx to delete on, deletion function]


	// Defers to the initialization of data on device-local onto the first frame (handled by UploadContext)
	std::queue<std::function<void(ID3D12GraphicsCommandList*)>> m_deferred_init_copies;

	std::unique_ptr<DXBufferGenericAllocator> m_committed_def_ator;
	std::unique_ptr<DXBufferGenericAllocator> m_committed_upload_ator;
};

