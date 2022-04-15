#pragma once

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

#define DET_ERR(msg) std::string("Error at line: (" + std::to_string(__LINE__) + ") in file: (" + __FILE__ + ")\n") + std::string(msg)

namespace utils
{
	std::wstring to_wstr(const std::string& str);
}

