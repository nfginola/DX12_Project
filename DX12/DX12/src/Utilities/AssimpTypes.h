#pragma once
#include <filesystem>
#include <variant>

struct AssimpMeshData
{
	unsigned int index_start = 0;
	unsigned int index_count = 0;
	unsigned int vertex_start = 0;
};

struct AssimpMaterialData
{
	struct PhongPaths
	{
		std::filesystem::path diffuse, normal, specular, opacity;
	};

	std::variant<PhongPaths> file_paths;
};