#ifndef SHADERINTEROP_COMMON_H
#define SHADERINTEROP_COMMON_H

#ifdef __cplusplus
#include <DirectXMath.h>

/*
	Allow for easily defining identical structure layout for C++ and HLSL.
	Map HLSL types to DirectXMath types
*/

using matrix = DirectX::XMMATRIX;
using float4x4 = DirectX::XMFLOAT4X4;
using float2 = DirectX::XMFLOAT2;
using float3 = DirectX::XMFLOAT3;
using float4 = DirectX::XMFLOAT4;
using uint = uint32_t;

#endif


#endif