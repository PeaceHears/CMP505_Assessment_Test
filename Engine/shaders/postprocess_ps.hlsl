Texture2D sceneTexture : register(t0);
SamplerState samplerState : register(s0);

cbuffer PostProcessBuffer : register(b0)
{
    int effectType;
    float vignetteIntensity;
    float2 padding;
}

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

float4 main(PixelInputType input) : SV_TARGET
{
    float4 textureColor = sceneTexture.Sample(samplerState, input.tex);
    
    switch (effectType)
    {
        case 1: // Invert
            textureColor.rgb = 1.0f - textureColor.rgb;
            break;
        
        case 2: // Grayscale
            float gray = dot(textureColor.rgb, float3(0.299f, 0.587f, 0.114f));
            textureColor.rgb = float3(gray, gray, gray);
            break;
        
        case 3: // Sepia
            float3 sepia = float3(
                dot(textureColor.rgb, float3(0.393f, 0.769f, 0.189f)),
                dot(textureColor.rgb, float3(0.349f, 0.686f, 0.168f)),
                dot(textureColor.rgb, float3(0.272f, 0.534f, 0.131f))
            );
            textureColor.rgb = saturate(sepia);
            break;
        
        case 4: // Vignette
            float2 vignetteCenter = float2(0.5f, 0.5f);
            float dist = distance(input.tex, vignetteCenter);
            textureColor.rgb *= smoothstep(0.75f, 0.75f - vignetteIntensity, dist);
            break;
    }
    
    return textureColor;
}