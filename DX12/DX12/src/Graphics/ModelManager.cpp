#include "pch.h"
#include "ModelManager.h"

#include "Utilities/AssimpLoader.h"

ModelManager::ModelManager(MeshManager* mesh_mgr, DXTextureManager* tex_mgr, DXBindlessManager* bindless_mgr) :
	m_mesh_mgr(mesh_mgr),
	m_tex_mgr(tex_mgr),
	m_bindless_mgr(bindless_mgr)
{
}

ModelHandle ModelManager::load_model(const ModelDesc& desc)
{
	auto [handle, res] = m_handles.get_next_free_handle();

	AssimpLoader loader(desc.rel_path);

	// Load mesh
	{
		MeshDesc md{};
		md.pos = utils::MemBlob(
			(void*)loader.get_positions().data(),
			loader.get_positions().size(),
			sizeof(loader.get_positions()[0]));

		md.uv = utils::MemBlob(
			(void*)loader.get_uvs().data(),
			loader.get_uvs().size(),
			sizeof(loader.get_uvs()[0]));

		md.indices = utils::MemBlob(
			(void*)loader.get_indices().data(),
			loader.get_indices().size(),
			sizeof(loader.get_indices()[0]));

		for (const auto& loaded_part : loader.get_meshes())
		{
			MeshPart part{};
			part.index_count = loaded_part.index_count;
			part.index_start = loaded_part.index_start;
			part.vertex_start = loaded_part.vertex_start;
			md.subsets.push_back(part);
		}
		res->mesh = m_mesh_mgr->create_mesh(md);
	}

	// Load Bindless Element
	{
		const auto& loaded_mats = loader.get_materials();
		for (const auto& loaded_mat : loaded_mats)
		{
			// load textures
			const auto& paths = std::get<AssimpMaterialData::PhongPaths>(loaded_mat.file_paths);
			DXTextureDesc td{};
			td.filepath = paths.diffuse;
			td.flag = TextureFlag::eSRGB;
			td.usage_cpu = UsageIntentCPU::eUpdateNever;
			td.usage_gpu = UsageIntentGPU::eReadMultipleTimesPerFrame;
			auto tex = m_tex_mgr->create_texture(td);

			// assemble bindless element
			// we want to remove duplicates on bindless elements
			DXBindlessDesc bd{};
			bd.diffuse_tex = tex;

			// Assemble material
			// BindlessHandle ==> Represents the arguments to the material (e.g varying textures, etc.)
			// Pipeline is naturally the parameters

			Material mat{};
			mat.resource = m_bindless_mgr->create_bindless(bd);
			mat.pso = desc.pso;
			res->mats.push_back(mat);
		}
	}

	return ModelHandle(handle);
}

void ModelManager::destroy_model(ModelHandle handle)
{
	assert(false);
}

const Model* ModelManager::get_model(ModelHandle handle)
{
	return m_handles.get_resource(handle.handle);
}
