#include "DXCommon.h"
#include "pch.h"

#include <stdexcept>

void ThrowIfFailed(HRESULT hr, const std::string& errMsg)
{
	if (FAILED(hr))
		throw std::runtime_error(errMsg);
}

