#include "ShaderInterop_Renderer.h"

struct Vertex
{
    float4 pos : POSITION;
    float2 uv : UV;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : UV;
};

static const Vertex verts[] =
{
    { { -0.5f, 0.5f, 0.f, 1.f }, { 0.f, 0.f } },
    { { 0.5f, -0.5f, 0.f, 1.f }, { 1.0f, 1.f } },
    { { -0.5f, -0.5f, 0.f, 1.f }, { 0.f, 1.f } },
    { { -0.5f, 0.5f, 0.f, 1.f }, { 0.f, 0.f } },
    { { 0.5f, 0.5f, 0.f, 1.f }, { 1.f, 0.f } },
    { { 0.5f, -0.5f, 0.f, 1.f }, { 1.0f, 1.f } }
};

StructuredBuffer<VertexPullPosition> vertices : register(t0, space5);
StructuredBuffer<VertexPullUV> uvs : register(t1, space5);
ConstantBuffer<InterOp_CameraData> cam_data : register(b7, space7);

struct PerDrawData
{
    float4x4 world_mat;
};
ConstantBuffer<PerDrawData> per_draw_data : register(b0, space0);

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

    output.pos = mul(cam_data.proj_mat, mul(cam_data.view_mat, mul(per_draw_data.world_mat, float4(vertices[vertID].position, 1.f))));
    output.uv = uvs[vertID].uv;
    
	return output;
}