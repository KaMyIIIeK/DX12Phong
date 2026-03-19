cbuffer cbPerObject : register(b0)
{
    float4x4 mWorldViewProj;
};

struct VSInput
{
    float3 Pos : POSITION;
    float4 Color : COLOR;
};

struct PSInput
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};

PSInput VS(VSInput vin)
{
    PSInput vout;
    vout.PosH = mul(float4(vin.Pos, 1.0f), mWorldViewProj);
    vout.Color = vin.Color;
    return vout;
}

float4 PS(PSInput pin) : SV_TARGET
{
    return pin.Color;
}