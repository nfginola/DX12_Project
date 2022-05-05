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
			auto paths = std::get<AssimpMaterialData::PhongPaths>(loaded_mat.file_paths);
			if (!paths.normal.has_filename())
				paths.normal = "textures/default_normal.png";
			if (!paths.opacity.has_filename())
				paths.opacity = "textures/default_opacity.png";
			if (!paths.specular.has_filename())
				paths.specular = "textures/default_specular.png";

			DXTextureDesc td{};
			td.filepath = paths.diffuse;
			td.flag = TextureFlag::eSRGB;
			td.usage_cpu = UsageIntentCPU::eUpdateNever;
			td.usage_gpu = UsageIntentGPU::eReadMultipleTimesPerFrame;
			auto diffuse = m_tex_mgr->create_texture(td);

			td.flag = TextureFlag::eNonSRGB;
			td.filepath = paths.normal;
			auto normal = m_tex_mgr->create_texture(td);

			td.flag = TextureFlag::eNonSRGB;
			td.filepath = paths.specular;
			auto specular = m_tex_mgr->create_texture(td);

			td.flag = TextureFlag::eNonSRGB;
			td.filepath = paths.opacity;
			auto opacity = m_tex_mgr->create_texture(td);

			// tex handles lost after this, we simply dont handle it since we are not handling dynamic removal
			// internal textures are cleaned up upon destruction
			
			// assemble bindless element
			DXBindlessDesc bd{};
			bd.diffuse_tex = diffuse;
			bd.normal_tex = normal;
			bd.opacity_tex = opacity;
			bd.specular_tex = specular;

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
