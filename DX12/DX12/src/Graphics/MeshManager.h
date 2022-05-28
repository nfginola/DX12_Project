#pragma once
#include "DX/DXBufferManager.h"
#include "Utilities/HandlePool.h"

struct MeshPart
{
	uint32_t index_start = 0;
	uint32_t index_count = 0;
	uint32_t vertex_start = 0;
};

struct Mesh
{
	std::vector<BufferHandle> vbs;
	BufferHandle ib;
	std::vector<MeshPart> parts;

	uint64_t handle = 0;
	void destroy() { };
};

struct MeshDesc
{
	utils::MemBlob pos, indices, uv, normals, tangents, bitangents;
	std::vector<MeshPart> subsets;

	bool valid() const
	{
		return !subsets.empty() && !pos.empty() && !indices.empty() && !uv.empty();
	}
};

struct RTAccelStructure
{
	BufferHandle blas, tlas, scratch, instances;

	uint64_t handle = 0;
	void destroy() { };
};

struct MeshHandle
{
public:
	MeshHandle() = default;
private:
	friend class MeshManager;
	MeshHandle(uint64_t handle_) : handle(handle_) {}
	uint64_t handle = 0;
};

struct RTMeshDesc
{
	MeshHandle mesh;
	DirectX::SimpleMath::Matrix world_mat;
};

class MeshManager
{
public:
	MeshManager(cptr<ID3D12Device> dev, DXBufferManager* buf_mgr, uint32_t max_FIF);
	~MeshManager() = default;

	MeshHandle create_mesh(const MeshDesc& desc);
	void destroy_mesh(MeshHandle handle);
	const Mesh* get_mesh(MeshHandle handle);

	// Single BLAS element
	void create_RT_accel_structure(const std::vector<RTMeshDesc>& geometries);

	// Variable rate BLAS elements
	void create_RT_accel_structure_v2(const std::vector<RTMeshDesc>& geometries);


	void create_RT_accel_structure_v3(const std::vector<RTMeshDesc>& geometries);



	void build_RT_accel_structure(ID3D12GraphicsCommandList5* cmdl);
	const RTAccelStructure* get_RT_accel_structure();

	void frame_begin(uint32_t frame_idx);
private:
	cptr<ID3D12Device5> m_dxr_dev;
	HandlePool<Mesh> m_handles;

	DXBufferManager* m_buf_mgr = nullptr;

	// active rt bufs
	RTAccelStructure m_rt_bufs;

	uint64_t m_frames_until_del = 0;
	RTAccelStructure m_old_rt_bufs;		// old to be deleted

	// temp for building
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_geom_descs;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC m_blas_d = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC m_tlas_d = {};

	uint32_t m_max_FIF;


	
	bool using_v2 = true;

	// V2
	struct BLASElement
	{
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geoms{};
		BufferHandle blas_buffer;
		BufferHandle scratch_buffer;
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO preb_info{};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blas_desc{};
		D3D12_RAYTRACING_INSTANCE_DESC instance_desc{};
		DirectX::SimpleMath::Matrix wm;		// assuming single instance for BLAS

		~BLASElement() = default;
	};

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC m_tlas_desc;

	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_instance_d;
	std::vector<std::unique_ptr<BLASElement>> m_blas_elements;

	BufferHandle m_single_tlas;
	BufferHandle m_single_scratch;
	BufferHandle m_single_instance;
};

