#include "ShaderInterop_Renderer.h"

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : UV;
};

struct BindlessIndex
{
    uint index;
};


SamplerState samp : register(s0, space0);

ConstantBuffer<BindlessIndex> bindless_index : register(b7, space0);        // Root arg constant with index to Bindless Element arr

// We dont need this if we use dynamic descriptor access
ConstantBuffer<BindlessElement> access_bufs[] : register(b0, space3);       // Bindless Element holds metadata or bindless resources


Texture2D bindless_texs[] : register(t0, space3);

float4 main(VSOut input) : SV_TARGET0
{    
    ConstantBuffer<BindlessElement> access_el = ResourceDescriptorHeap[bindless_index.index];
    //BindlessElement bindless_el = access_bufs[bindless_index.index];
    
    float3 col = bindless_texs[access_el.diffuse_idx].Sample(samp, input.uv).rgb;
      
    col = pow(col, (1.0 / 2.2).xxx); // Gamma correct
    
    return float4(col, 1.f);
}