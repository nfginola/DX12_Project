#include "pch.h"
#include <filesystem>

std::wstring utils::to_wstr(const std::string& str)
{
    std::filesystem::path path(str);
    return path.c_str();
}
