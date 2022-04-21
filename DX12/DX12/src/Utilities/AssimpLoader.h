#pragma once
#include "AssimpTypes.h"

/*
	NOMINMAX preprocessor definition has to be set. windows.h min clashes with C++ headers
*/

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

class AssimpLoader
{
private:

public:
	AssimpLoader() = delete;
	AssimpLoader(const std::filesystem::path& fpath);

	/*
		Vertex data are returned in non-interleaved form
		Packing to interleaved form is up to the end user
	*/
	const std::vector<aiVector3D>& get_positions() { return m_positions; }
	const std::vector<aiVector2D>& get_uvs() { return m_uvs; }
	const std::vector<aiVector3D>& get_normals() { return m_normals; }
	const std::vector<aiVector3D>& get_tangents() { return m_tangents; }
	const std::vector<aiVector3D>& get_bitangents() { return m_bitangents; }

	const std::vector<uint32_t>& get_indices() { return m_indices; }

	const std::vector<AssimpMeshData>& get_meshes() { return m_meshes; }
	const std::vector<AssimpMaterialData>& get_materials() { return m_materials; }

private:
	void process_material(aiMaterial* material, const aiScene* scene);
	void process_mesh(aiMesh* mesh, const aiScene* scene);
	void process_node(aiNode* node, const aiScene* scene);

private:
	std::filesystem::path m_directory;

	std::vector<aiVector3D> m_positions;
	std::vector<aiVector2D> m_uvs;
	std::vector<aiVector3D> m_normals;
	std::vector<aiVector3D> m_tangents;
	std::vector<aiVector3D> m_bitangents;

	std::vector<uint32_t> m_indices;

	// there is a 1:1 mapping between meshes and materials
	std::vector<AssimpMeshData> m_meshes;
	std::vector<AssimpMaterialData> m_materials;


};

