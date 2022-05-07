#pragma once

#if defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#endif

#include <wrl/client.h>		// ComPtr
template <typename T>
using cptr = Microsoft::WRL::ComPtr<T>;

#include <memory>		// Smart pointers
template <typename T>
using sptr = std::shared_ptr<T>;
template <typename T>
using uptr = std::unique_ptr<T>;

#include <stdint.h>

#include <string>
#include <assert.h>
#include <iostream>
#include <functional>
#include <filesystem>
#include <array>
#include <fstream>

#include "DXTK/SimpleMath.h"

// FMT
#include "fmt/core.h"
#include "fmt/color.h"
#include <fmt/os.h>


#define DET_ERR(msg) std::string("Error at line: (" + std::to_string(__LINE__) + ") in file: (" + __FILE__ + ")\n") + std::string(msg)


namespace utils
{
	std::wstring to_wstr(const std::string& str);

	struct MemBlob
	{
		MemBlob() = default;
		MemBlob(void* data_, size_t count_, size_t stride_) :
			data(data_),
			count((uint32_t)count_),
			stride((uint32_t)stride_) {
			total_size = count * stride;
		}

		bool empty() const { return data == nullptr || count == 0 || stride == 0; }

		void* data = nullptr;
		uint32_t count = 0;
		uint32_t stride = 0;
		uint32_t total_size = 0;
	};


	std::vector<uint8_t> read_file(const std::filesystem::path& filePath);
	void constrained_incr(float& num, float min, float max);
	void constrained_decr(float& num, float min, float max);
	float constrained_add(float lh, float rh, float min, float max);

}

