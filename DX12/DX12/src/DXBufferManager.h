#pragma once
#include <variant>
#include "Utilities/HandlePool.h"
#include "DXCommon.h"

// Handle constant data suballocations
#include "DXConstantSuballocation.h"


// Allocation algorithms for constant data management
class DXConstantRingBuffer;
class DXConstantStaticBuffer;

// Strongly typed handle to a buffer for the application to hold on to
struct BufferHandle
{
private:
	BufferHandle(uint64_t handle_) : handle(handle_) {}
	friend class DXBufferManager;
	uint64_t handle;
};


struct DXBufferDesc
{
	UsageIntentCPU usage_cpu = UsageIntentCPU::eInvalid;
	UsageIntentGPU usage_gpu = UsageIntentGPU::eInvalid;

	uint32_t element_size = 0;
	uint32_t element_count = 0;

	// Init. data (optional)
	void* data = nullptr;
	size_t data_size = 0;
};

class DXBufferManager
{

public:
	DXBufferManager(Microsoft::WRL::ComPtr<ID3D12Device> dev);
	~DXBufferManager();

	BufferHandle create_buffer(const DXBufferDesc& desc);
	void destroy_buffer(BufferHandle hdl);

	void upload_data(void* data, size_t size, BufferHandle hdl);
	
	void frame_begin(uint32_t frame_idx);

	// join-point for potential async copies that need completion guarantee prior to pipeline invocations (e.g draws)
	void wait_on_queue(ID3D12CommandQueue* queue);


	/*
		support copying operations to-and-from buffers here as API
	*/
	
	
private:

	struct ConstantAccessBufferMD
	{
		DXConstantSuballocation* alloc;
		bool transient = false;
	};

	struct ShaderAccessBufferMD
	{
		int a = 1;	// to implement
	};

	struct UnorderedAccessBufferMD
	{
		int a = 1;	 // to implement
	};

	struct InternalBufferResource
	{
		template <typename T>
		T& get_metadata()
		{
			T* to_ret = nullptr;
			try
			{
				to_ret = &(std::get<T>(metadata));
			}
			catch (const std::bad_variant_access& e)
			{
				std::cout << e.what() << "\n";
				assert(false);		// programmer error!
			}
			return *to_ret;
		}

		std::variant<
			ConstantAccessBufferMD,
			ShaderAccessBufferMD,
			UnorderedAccessBufferMD> metadata;

		uint64_t total_requested_size = 0;
		UsageIntentGPU usage_gpu = UsageIntentGPU::eInvalid;	// Used to idenetify metadata variant
		
		uint64_t handle = 0;
		void destroy() { /* destruction is done externally */ }
	};

private:
	InternalBufferResource* grab_constant_memory(const DXBufferDesc& desc);

private:
	Microsoft::WRL::ComPtr<ID3D12Device> m_dev;

	HandlePool<InternalBufferResource> m_handles;

	std::unique_ptr<DXConstantRingBuffer> m_constant_ring_buf;
	std::unique_ptr<DXConstantStaticBuffer> m_constant_persistent_buf;




};

