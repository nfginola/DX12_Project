struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : UV;
};

struct TestCB
{
    float4 color;
};

ConstantBuffer<TestCB> cb : register(b0, space0);

float4 main(VSOut input) : SV_TARGET0
{
    // just test uv coords for now
    float3 col = float3(input.uv, 0.f);
    
    col = pow(col, (1.0 / 2.2).xxx); // Gamma correct
    
    return float4(col, 1.f);
}