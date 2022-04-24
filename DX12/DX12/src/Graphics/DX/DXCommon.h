#pragma once

#include <string>

#include <d3d12.h>
#include <dxgi1_4.h>		// For CreateDXGIFactory2
#include <dxgi1_6.h>		// Querying high perf GPU (e.g dGPU instead of iGPU on laptop)
#include <dxgidebug.h>		// DXGI debug device (requires linking to dxiguid.lib)

#include "d3dx12.h"

void ThrowIfFailed(HRESULT hr, const std::string& errMsg);

enum class RootArgDest
{
	eInvalid,
	eGraphics,
	eCompute
};

enum class UsageIntentCPU
{
	eInvalid,

	eUpdateNever,					// Maps to device-local memory
	eUpdateSometimes,				// Maps to device-local memory
	eUpdateOnce,					// Upload if GPU intends to read once OR device-local if GPU intends to read many times
	eUpdateMultipleTimesPerFrame,	// Upload if GPU intends to read onece OR device-local if GPU intends to read many times


	eUpdateOnceOrMorePerFrame	// Maps to host-local memory (upload heap)
};

enum class UsageIntentGPU
{
	eInvalid,

	eReadOncePerFrame,
	eReadMultipleTimesPerFrame,

	// We dont need these below since they are handled outside (by descriptors, which are orthogonal to the resources)
	eVertexRead,	// Maps to VBV
	eIndexRead,		// Maps to IBV
	eConstantRead,	// Maps to CBV
	eShaderRead,	// Maps to SRV
	eReadWrite		// Maps to UAV
};

enum class ShaderModel
{
	e6_0
};

enum class ShaderType
{
	eVertex,
	ePixel,
	eCompute,
	eHull,
	eDomain,
	eGeometry
};

enum class DepthFormat 
{ 
	eD32, 
	eD32_S8, 
	eD24_S8, 
	eD16
};

