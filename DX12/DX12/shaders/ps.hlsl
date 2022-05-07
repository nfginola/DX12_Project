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

// Constant
struct BindlessIndex
{
    uint index;
};
ConstantBuffer<BindlessIndex> bindless_index : register(b7, space0);        // Root arg constant with index to Bindless Element arr

SamplerState samp : register(s0, space0);

Texture2D bindless_texs[] : register(t0, space3);

ConstantBuffer<InterOp_Settings> settings : register(b0, space21);
ConstantBuffer<InterOp_CameraData> cam_data : register(b7, space7);


RaytracingAccelerationStructure accel_struct : register(t3, space0);



float3 get_normal(float3 tangent, float3 bitangent, float3 input_normal, float3 tan_space_nor);

float4 main(VSOut input) : SV_TARGET0
{       
    ConstantBuffer<BindlessElement> access_el = ResourceDescriptorHeap[bindless_index.index];
   
    // textures are grouped
    const static uint num_tex = 4;
    float4 diffuse = bindless_texs[access_el.diffuse_idx].Sample(samp, input.uv);
    float3 col = 0.f.xxx;
    
    // alpha from opacity mask
    float alpha = bindless_texs[access_el.opacity_idx].Sample(samp, input.uv).r;
    // handle opacity in both cases where it comes as a texture or if its implicitly in the diffuse alpha part!
    alpha = min(alpha, diffuse.a);
    
    // grab normal
    float3 tan_sp_nor = bindless_texs[access_el.normal_idx].Sample(samp, input.uv).rgb;
    float3 normal = normalize(get_normal(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal), tan_sp_nor));
        
    // do some lighting calculation
    //float3 light_dir = normalize(float3(0.3f, -0.8f, 0.7f));
    float3 light_dir = normalize(settings.dir_light);
    float3 light_col = 1.f.xxx;
    float3 diffuse_contrib = saturate(dot(-light_dir, normal) * light_col);
    
  
    // Blinn Phong specular
    //float3 spec = bindless_texs[access_el.diffuse_idx * num_tex + 2].Sample(samp, input.uv).rgb;
    
    float3 frag_to_cam = normalize((-cam_data.view_mat[3].xyz) - input.world_pos);
    float3 frag_to_light = normalize(-light_dir);
    float3 halfway = normalize(frag_to_cam + frag_to_light);
    
    float3 specular = pow(max(dot(normal, halfway), 0.f), access_el.specular) * light_col;
    
    // if we set specular as < 0, we want it to act as no specular contrib
    specular = saturate(min(specular, access_el.specular.xxx));

        
    float3 diffuse_col = diffuse.rgb * diffuse_contrib;
    col = diffuse_col + specular;
    
    
    if (settings.raytrace_on == 1)
    {
        RayQuery<RAY_FLAG_CULL_NON_OPAQUE |
                RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH > ray_query;
    
        uint ray_flags = 0;
        uint instance_mask = 0xFF;
        float shadow_fac = 1.f;
    
        RayDesc ray = (RayDesc) 0;
        ray.TMin = 1e-5f;
        ray.TMax = 1500.f;
        ray.Direction = normalize(-light_dir);
        ray.Origin = input.world_pos + settings.shadow_bias * ray.Direction; // add bias towards light dir to minimize acne
    
        ray_query.TraceRayInline(accel_struct, ray_flags, instance_mask, ray);
    
        ray_query.Proceed();
    
        if (ray_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {             
            shadow_fac = 0.04f;
        }
        
        col *= shadow_fac;
    }

    

    
    
    
    
    
    
    
    
    
    // Gamma correct
    col = pow(col, (1.0 / 2.2).xxx);
    
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
    
    // Toggle normal map use
    return map_nor_world * settings.normal_map_on + input_normal * (1.f - settings.normal_map_on);
}