#include "ShaderInterop_Renderer.h"

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : UV;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float3 world_pos : WORLDPOS;
};

StructuredBuffer<VertexPullPosition> vertices : register(t0, space5);
StructuredBuffer<VertexPullUV> uvs : register(t1, space5);
StructuredBuffer<VertexPullNormal> normals : register(t2, space5);
StructuredBuffer<VertexPullTangent> tangents : register(t3, space5);
StructuredBuffer<VertexPullBitangent> bitangents : register(t4, space5);

ConstantBuffer<InterOp_CameraData> cam_data : register(b7, space7);

struct PerDrawData
{
    float4x4 world_mat;
};
ConstantBuffer<PerDrawData> per_draw_data : register(b0, space0);


// Constant
struct VertOffset
{
    uint offset;
};
ConstantBuffer<VertOffset> vert_offset : register(b8, space0);


VSOut main( uint vertID : SV_VertexID )
{
    VSOut output = (VSOut) 0;
    
    // manually pass in vb offset
    vertID += vert_offset.offset;

    float3 loc_pos = vertices[vertID].position;
    float3 tangent = tangents[vertID].tangent;
    float3 bitangent = bitangents[vertID].bitangent;
    float3 normal = normals[vertID].normal;
    float2 uv = uvs[vertID].uv;
    
    output.world_pos = mul(per_draw_data.world_mat, float4(loc_pos, 1.f)).xyz;
    output.pos = mul(cam_data.proj_mat, mul(cam_data.view_mat, float4(output.world_pos, 1.f)));
    output.normal = normalize(mul(per_draw_data.world_mat, float4(normal, 0.f)).xyz);
    output.tangent = normalize(mul(per_draw_data.world_mat, float4(tangent, 0.f)).xyz);
    output.bitangent = normalize(mul(per_draw_data.world_mat, float4(bitangent, 0.f)).xyz);
    output.uv = uv;
    
	return output;
}