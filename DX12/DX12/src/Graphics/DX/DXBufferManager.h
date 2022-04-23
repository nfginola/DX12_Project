#pragma once
#include <variant>
#include "Utilities/HandlePool.h"
#include "Graphics/DX/DXCommon.h"


#include "Buffer/DXBufferPoolAllocator.h"
#include "Buffer/DXBufferRingPoolAllocator.h"

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

	void frame_begin(uint32_t frame_idx);

	BufferHandle create_buffer(const DXBufferDesc& desc);
	void destroy_buffer(BufferHandle hdl);

	// Maybe should be moved to an DXCopyManager?					// Handles CPU-GPU and GPU-GPU copies and handles potentialy Waits associated with GPU-GPU copies
	void upload_data(void* data, size_t size, BufferHandle hdl);
	// join-point for potential async copies that need completion guarantee prior to pipeline invocations (e.g draws)
	//void wait_on_queue(ID3D12CommandQueue* queue);
	 


	// Maybe should be moved to a DXRootArgBinder which has a reference to a BufferManager and a TextureManager?
	void bind_as_direct_arg(ID3D12GraphicsCommandList* cmdl, BufferHandle buf, UINT param_idx, RootArgDest dest);



	
private:

	struct ConstantAccessBufferMD
	{
		DXBufferSuballocation* alloc;
		bool transient = false;
	};

	// Due to time constraints, we will upgrade suballocations later for other types
	// Since the other types require arbitrarily sized allocations
	
	// We cant use the Pool Allocator for Structured Buffers, because we essentially need a new MemPool per new Structured Buffer (which is equivalent to a Committed Resource)
	// If we want to do properly, we'd need a generic buffer suballocator algorithm
	// --> alloc(10 * sizeof(SomeStruct)  
	
	// Likely structured: Make Committed resource OR Placed 
	struct ShaderAccessBufferMD
	{
		int a = 1;	// to implement
	};

	// Make Committed resource OR Placed.
	struct UnorderedAccessBufferMD
	{
		int a = 1;	 // to implement
	};

	struct InternalBufferResource
	{
		std::variant<
			ConstantAccessBufferMD,
			ShaderAccessBufferMD,
			UnorderedAccessBufferMD> metadata;

		uint64_t total_requested_size = 0;
		UsageIntentGPU usage_gpu = UsageIntentGPU::eInvalid;	// Used to idenetify metadata variant
		
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

		uint64_t handle = 0;
		void destroy() { /* destruction is done externally */ }
	};

private:
	InternalBufferResource* grab_constant_memory(const DXBufferDesc& desc);

private:
	Microsoft::WRL::ComPtr<ID3D12Device> m_dev;
	HandlePool<InternalBufferResource> m_handles;

	/*
		For buffers with persistent locations,
		we should create a View for them according to the usage specified by the user

		Persistent Buffers work well with Descriptors, since they are not frequently updated (so they 

		We should be able to batch resources to contiguously store in a CPU descriptor heap;

		TexHdl amb
		TexHdl diff
		TexHdl spec

		DescHdl d_hdl = desc_mgr.allocate({ amb, diff, spec })		--> Group is required to be of same GPU access (e.g all SRVs)

		BufHdl b1
		BufHdl b2
		BufHdl b3

		DescHdl d_hdl2 = desc.mgr.allocate({ b1, b2, b3 })		--> Same as above (e.g they all must either be SRV or CBV)
		--> Table with 3

		--> If transient buffer/texture given --> Assert


		Perhaps this should be an external thing that the app needs to do manually:
			Meaning app has to:
				1) Create resource(s)
				2) Create descriptors for the resource(s) manually through the manager
					--> This gives flexibility to placement of resources in the CPU visible descriptor heap!
					--> If we automate this, it has to be tied with resource creation, which is arbitrary,
						thus it is impossible to be flexible and smart with the descriptor management on the CPU only heap!


		BindlessDescriptorManager should MAKE USE OF a DescriptorManager --> This is a specialized case
		- Holds the most recent version of the resources
		- Tracks changes with details (knows how the GPU version is outdated and can copy appropriately only the part that changes)
		- Holds persistent Constant Buffers for each material
			- When a descriptor (table) is unloaded, the constant buffer is also unloaded
				- Therefore, that descriptor will not be accessible on subsequent frames after this removal.


		--> If we want descriptor data --> Go to descriptor managers
		--> If we want immediate binds --> Grab immediate resource info from BufferHandle / TextureHandle

		
		[  Static,      Dynamic (ring buffered)  ]

		Static:
		[  50 Reserved (e.g ImGUI),  1000 (100 CBVs, 850 SRVs, 50 UAVs),     Dynamic Descriptors   ]







		

	
	
	
	*/

	std::unique_ptr<DXBufferRingPoolAllocator> m_constant_ring_buf;
	std::unique_ptr<DXBufferPoolAllocator> m_constant_persistent_buf;


};

