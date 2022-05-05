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
Texture2D bindless_texs[] : register(t0, space3);

float4 main(VSOut input) : SV_TARGET0
{    
    ConstantBuffer<BindlessElement> access_el = ResourceDescriptorHeap[bindless_index.index];
    
    // textures are grouped
    const static uint num_tex = 4;
    float3 col = bindless_texs[access_el.diffuse_idx * num_tex].Sample(samp, input.uv).rgb;
      
    // alpha from opacity mask
    float alpha = bindless_texs[access_el.diffuse_idx * num_tex + 3].Sample(samp, input.uv).r;
    
    col = pow(col, (1.0 / 2.2).xxx); // Gamma correct
    
    return float4(col, alpha);    
}