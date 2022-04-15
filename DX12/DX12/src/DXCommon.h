#pragma once

#include <string>

#include <d3d12.h>
#include <dxgi1_4.h>		// For CreateDXGIFactory2
#include <dxgi1_6.h>		// Querying high perf GPU (e.g dGPU instead of iGPU on laptop)
#include <dxgidebug.h>		// DXGI debug device (requires linking to dxiguid.lib)

#include "d3dx12.h"


void ThrowIfFailed(HRESULT hr, const std::string& errMsg);

enum class ShaderModel
{
	e6_0
};

enum class ShaderType
{
	eVertex,
	ePixel,
	eCompute,
};

