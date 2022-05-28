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
	BufferHandle tlas;

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
	enum class RTBuildSetting
	{
		eBLASPerSubmesh,
		eBLASPerModel,
		eBLASVariableSubmesh
	};

	struct RTSceneData
	{
		int tlas_count;
		int blas_count;
		int submesh_per_blas;
		int total_verts;
	};

public:
	MeshManager(cptr<ID3D12Device> dev, DXBufferManager* buf_mgr, uint32_t max_FIF);
	~MeshManager() = default;

	MeshHandle create_mesh(const MeshDesc& desc);
	void destroy_mesh(MeshHandle handle);
	const Mesh* get_mesh(MeshHandle handle);

	// submesh per BLAS only used if eBLASVariableSubmesh is on
	void create_RT_accel_structure_v3(const std::vector<RTMeshDesc>& geometries, RTBuildSetting setting = RTBuildSetting::eBLASPerModel, UINT submesh_per_BLAS = 0);
	void build_RT_accel_structure(ID3D12GraphicsCommandList5* cmdl);
	const RTAccelStructure* get_RT_accel_structure();



	void frame_begin(uint32_t frame_idx);
private:
	cptr<ID3D12Device5> m_dxr_dev;
	HandlePool<Mesh> m_handles;

	DXBufferManager* m_buf_mgr = nullptr;


	uint64_t m_frames_until_del = 0;

	uint32_t m_max_FIF;


	RTAccelStructure m_rt_bufs;
	RTSceneData scene_data;

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


	struct TLASElement
	{
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlas_desc;

		std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instance_d;
		std::vector<std::unique_ptr<BLASElement>> blas_elements;

		BufferHandle single_tlas;
		BufferHandle single_scratch;			// Used for TLAS only
		BufferHandle single_instance;

		void clear_resources(DXBufferManager* mgr)
		{
			mgr->destroy_buffer(single_tlas);
			mgr->destroy_buffer(single_scratch);
			mgr->destroy_buffer(single_instance);

			for (auto& blas_el : blas_elements)
			{
				mgr->destroy_buffer(blas_el->blas_buffer);
				mgr->destroy_buffer(blas_el->scratch_buffer);
			}
		}

	};

	std::unique_ptr<TLASElement> m_tlas_element;
	std::unique_ptr<TLASElement> m_tlas_to_delete;
};

