#include "pch.h"
#include "AssimpLoader.h"
#include "DXTK/SimpleMath.h"

using namespace DirectX::SimpleMath;

AssimpLoader::AssimpLoader(const std::filesystem::path& fpath) :
	m_directory(std::filesystem::path(fpath.parent_path().string() + "/"))
{
	// Load assimp scene
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(
		fpath.relative_path().string().c_str(),
		aiProcess_Triangulate |

		// For Direct3D
		aiProcess_ConvertToLeftHanded |
		//aiProcess_FlipUVs |					// (0, 0) is top left
		//aiProcess_FlipWindingOrder |			// D3D front face is CW

		aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace |

		// Extra flags (http://assimp.sourceforge.net/lib_html/postprocess_8h.html#a64795260b95f5a4b3f3dc1be4f52e410a444a6c9d8b63e6dc9e1e2e1edd3cbcd4)
		aiProcess_JoinIdenticalVertices |
		aiProcess_ImproveCacheLocality
	);

	if (!scene)
		assert(false);

	// Get total amount of vertices and pre-allocate memory
	unsigned int total_verts = 0;
	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
		total_verts += scene->mMeshes[i]->mNumVertices;

	m_positions.reserve(total_verts);
	m_uvs.reserve(total_verts);
	m_normals.reserve(total_verts);
	m_indices.reserve(total_verts);
	m_tangents.reserve(total_verts);
	m_bitangents.reserve(total_verts);
	m_meshes.reserve(scene->mNumMeshes);

	// Reserve memory for materials
	m_materials.reserve(scene->mNumMaterials);

	// Start processing scene
	process_node(scene->mRootNode, scene);
}

void AssimpLoader::process_material(aiMaterial* material, const aiScene* scene)
{
	aiReturn ret;

	// Get file paths
	aiString diffuse, normal, opacity, specular;

	material->GetTexture(aiTextureType_DIFFUSE, 0, &diffuse);

	// Try getting normal map, if unsuccessful, get from height
	ret = material->GetTexture(aiTextureType_NORMALS, 0, &normal);
	if (ret != aiReturn_SUCCESS)
		material->GetTexture(aiTextureType_HEIGHT, 0, &normal);

	material->GetTexture(aiTextureType_OPACITY, 0, &opacity);
	material->GetTexture(aiTextureType_SPECULAR, 0, &specular);

	// Save
	AssimpMaterialData::PhongPaths paths;

	// Set directory
	paths.diffuse = m_directory;
	paths.normal = m_directory;
	paths.specular = m_directory;
	paths.opacity = m_directory;

	// Append file names
	paths.diffuse += diffuse.C_Str();
	paths.normal += normal.C_Str();
	paths.specular += specular.C_Str();
	paths.opacity += opacity.C_Str();

	AssimpMaterialData data;
	data.file_paths = paths;
	m_materials.push_back(data);
}

void AssimpLoader::process_mesh(aiMesh* mesh, const aiScene* scene)
{
	/*
		Get all the the relevant vertex data from this mesh
	*/
	UINT vertex_start = (UINT)m_positions.size();
	for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
	{
		m_positions.push_back(mesh->mVertices[i]);
		m_normals.push_back(mesh->mNormals[i]);

		if (mesh->mTextureCoords[0])
			m_uvs.push_back({ mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y });

		if (mesh->mTangents)
			m_tangents.push_back(mesh->mTangents[i]);

		if (mesh->mBitangents)
			m_bitangents.push_back(mesh->mBitangents[i]);
	}

	/*
		Save where this mesh starts in the index buffer
	*/
	//size_t index_start = (std::max)((int64_t)m_indices.size() - 1, (int64_t)0);
	auto index_start = (UINT)m_indices.size();

	/*
		Go over this meshes faces and extract indices.
		If triangulation is enabled, each face should have 3 vertices.
	*/
	unsigned int index_count = 0;
	for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; ++j)
		{
			m_indices.push_back(face.mIndices[j]);
			++index_count;
		}
	}

	AssimpMeshData amd;
	amd.index_start = index_start;
	amd.index_count = index_count;

	/*
		Indices are local to the mesh!
		Thus requiring the offset to this mesh in the joint vertex buffer
	*/
	amd.vertex_start = vertex_start;
	m_meshes.push_back(amd);
}

void AssimpLoader::process_node(aiNode* node, const aiScene* scene)
{
	// Process all meshes in this node
	for (unsigned int i = 0; i < node->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

		// Get material for this mesh
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		process_mesh(mesh, scene);
		process_material(material, scene);
	}

	// Recursively process all child nodes and process meshes in them.
	for (unsigned int i = 0; i < node->mNumChildren; ++i)
		process_node(node->mChildren[i], scene);
}
