#include "ShaderInterop_Renderer.h"

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : UV;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

struct BindlessIndex
{
    uint index;
};

float3 get_normal(float3 tangent, float3 bitangent, float3 input_normal, float3 tan_space_nor);

SamplerState samp : register(s0, space0);

ConstantBuffer<BindlessIndex> bindless_index : register(b7, space0);        // Root arg constant with index to Bindless Element arr
Texture2D bindless_texs[] : register(t0, space3);

float4 main(VSOut input) : SV_TARGET0
{    
    ConstantBuffer<BindlessElement> access_el = ResourceDescriptorHeap[bindless_index.index];
   
    // textures are grouped
    const static uint num_tex = 4;
    float4 diffuse = bindless_texs[access_el.diffuse_idx * num_tex].Sample(samp, input.uv);
    float3 col = diffuse.rgb;
    
    // alpha from opacity mask
    float alpha = bindless_texs[access_el.diffuse_idx * num_tex + 3].Sample(samp, input.uv).r;

    // handle opacity in both cases where it comes as a texture or if its implicitly in the diffuse alpha part!
    alpha = min(alpha, diffuse.a);
    
    // grab normal
    float3 tan_sp_nor = bindless_texs[access_el.diffuse_idx * num_tex + 1].Sample(samp, input.uv).rgb;
    float3 normal = normalize(get_normal(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal), tan_sp_nor));
    
    // do some lighting calculation
    



    
    col = pow(col, (1.0 / 2.2).xxx); // Gamma correct
    
    return float4(col, alpha);    
}



float3 get_normal(float3 tangent, float3 bitangent, float3 input_normal, float3 tan_space_nor)
{               
    float3x3 tbn = float3x3(tangent, bitangent, input_normal); // matrix to orient our tangent space normal with
    tbn = transpose(tbn);
    
    // Normal map is in [0, 1] space so we need to transform it to [-1, 1] space
    float3 mapped_space_nor = normalize(tan_space_nor * 2.f - 1.f);
     
    // Orient the tangent space correctly in world space
    float3 map_nor_world = normalize(mul(tbn, mapped_space_nor));
    
       
    float normal_map_on = 1.f;
    
    // Toggle normal map use
    return map_nor_world * normal_map_on + input_normal * (1.f - normal_map_on);
}