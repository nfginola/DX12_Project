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


struct TestCB
{
    float3 offset;
};

ConstantBuffer<TestCB> cb : register(b0, space0);

//StructuredBuffer<VertexPullElement> vertices : register(t0, space5);
StructuredBuffer<VertexPullPosition> vertices : register(t0, space5);
StructuredBuffer<VertexPullUV> uvs : register(t1, space5);


VSOut main( uint vertID : SV_VertexID )
{
    VSOut output = (VSOut) 0;
    
    //output.pos = mul(cb.projMat, mul(cb.viewMat, mul(cb.worldMat, float4(verts[vertID].pos.xyz, 1.f))));
    //output.pos = float4(verts[vertID].pos.xyz, 1.f);
    //output.uv = verts[vertID].uv;
    output.pos = float4(vertices[vertID].position + cb.offset, 1.f);
    output.uv = uvs[vertID].uv, 1.f;
    
	return output;
}