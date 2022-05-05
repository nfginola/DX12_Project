#include "pch.h"
#include <filesystem>

namespace utils
{
	std::wstring to_wstr(const std::string& str)
	{
		std::filesystem::path path(str);
		return path.c_str();
	}

	std::vector<uint8_t> read_file(const std::filesystem::path& filePath)
	{
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);
		if (!file.is_open())
			assert(false);

		size_t file_size = static_cast<size_t>(file.tellg());
		std::vector<uint8_t> buffer(file_size);

		file.seekg(0);
		file.read((char*)buffer.data(), file_size);

		file.close();
		return buffer;
	}

	void constrained_incr(float& num, float min, float max)
	{
		++num;
		num = (std::max)((std::min)(num, max), min);
	}

	void constrained_decr(float& num, float min, float max)
	{
		--num;
		num = (std::max)((std::min)(num, max), min);
	}

	float constrained_add(float lh, float rh, float min, float max)
	{
		lh += rh;
		lh = (std::max)((std::min)(lh, max), min);
		return lh;
	}
}

