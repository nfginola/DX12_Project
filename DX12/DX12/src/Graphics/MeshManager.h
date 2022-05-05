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

struct MeshHandle
{
public:
	MeshHandle() = default;
private:
	friend class MeshManager;
	MeshHandle(uint64_t handle_) : handle(handle_) {}
	uint64_t handle = 0;
};

class MeshManager
{
public:
	MeshManager(DXBufferManager* buf_mgr);
	~MeshManager() = default;

	MeshHandle create_mesh(const MeshDesc& desc);
	void destroy_mesh(MeshHandle handle);

	const Mesh* get_mesh(MeshHandle handle);

private:
	HandlePool<Mesh> m_handles;

	DXBufferManager* m_buf_mgr = nullptr;
	
};

