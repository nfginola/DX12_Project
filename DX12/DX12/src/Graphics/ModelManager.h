#pragma once
#include "Utilities/HandlePool.h"
#include "Graphics/MeshManager.h"
#include "DX/DXTextureManager.h"
#include "DX/DXBindlessManager.h"

struct Material
{
	cptr<ID3D12PipelineState> pso;
	BindlessHandle resource;
};

struct Model
{
	MeshHandle mesh;
	std::vector<Material> mats;

	uint64_t handle = 0;
	void destroy() { };
};

struct ModelDesc
{
	std::filesystem::path rel_path;
	cptr<ID3D12PipelineState> pso;
};

struct ModelHandle
{
public:
	ModelHandle() = default;
private:
	ModelHandle(uint64_t handle_) : handle(handle_) {}
	friend class ModelManager;
	uint64_t handle = 0;
};

class ModelManager
{
public:
	ModelManager(MeshManager* mesh_mgr, DXTextureManager* tex_mgr, DXBindlessManager* bindless_mgr);
	~ModelManager() = default;

	ModelHandle load_model(const ModelDesc& desc);
	void destroy_model(ModelHandle handle);

	const Model* get_model(ModelHandle handle);

private:
	HandlePool<Model> m_handles;

	MeshManager* m_mesh_mgr = nullptr;
	DXTextureManager* m_tex_mgr = nullptr;
	DXBindlessManager* m_bindless_mgr = nullptr;


};

