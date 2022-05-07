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
	MeshManager(cptr<ID3D12Device> dev, DXBufferManager* buf_mgr);
	~MeshManager() = default;

	MeshHandle create_mesh(const MeshDesc& desc);
	void destroy_mesh(MeshHandle handle);
	const Mesh* get_mesh(MeshHandle handle);

	// Each mesh is mapped to one BLAS element
	void create_RT_accel_structure(const std::vector<RTMeshDesc>& geometries);
	void build_RT_accel_structure(ID3D12GraphicsCommandList5* cmdl);
	const RTAccelStructure* get_RT_accel_structure();

private:
	cptr<ID3D12Device5> m_dxr_dev;
	HandlePool<Mesh> m_handles;

	DXBufferManager* m_buf_mgr = nullptr;
	RTAccelStructure m_rt_bufs{};

	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geom_descs;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blas_d = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlas_d = {};
};

