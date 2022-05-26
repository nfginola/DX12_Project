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

struct InterOp_Settings
{
	float3 dir_light;
	float shadow_bias;
	int normal_map_on;
	int raytrace_on;
};


struct BindlessElement
{
	uint diffuse_idx;
	uint normal_idx;
	uint specular_idx;
	uint opacity_idx;

	float specular;		// spec exp
	/*
		spec idx
		amb idx
		tint color, etc.	
	*/
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

struct VertexPullNormal
{
	float3 normal;
};

struct VertexPullTangent
{
	float3 tangent;
};

struct VertexPullBitangent
{
	float3 bitangent;
};



#endif