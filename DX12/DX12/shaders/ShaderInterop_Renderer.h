#ifndef SHADERINTEROP_RENDERER_H
#define SHADERINTEROP_RENDERER_H
#include "ShaderInterop_Common.h"

// On upload heap since view mat changes all the time? (Should we separate projection matrix?)
struct InterOp_CameraData
{
	float4x4 view_mat;
	float4x4 proj_mat;
};

struct InterOp_DirectionalLightData
{
	float3 direction;
	float3 color;
};

struct BindlessElement
{
	uint diffuse_idx;
};

// Interleaved buffer
struct VertexPullElement
{
	float3 position;
	float2 uv;
};

// Non-interleaved buffers
struct VertexPullPosition
{
	float3 position;
};

struct VertexPullUV
{
	float2 uv;
};


#endif