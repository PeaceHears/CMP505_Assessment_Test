struct VertexInputType
{
    uint vertexId : SV_VertexID;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

PixelInputType main(VertexInputType input)
{
    PixelInputType output;
    
    // Generate a full-screen triangle
    output.position.x = (float) (input.vertexId & 1) * 4.0 - 1.0;
    output.position.y = (float) (input.vertexId >> 1) * 4.0 - 1.0;
    output.position.z = 0.0;
    output.position.w = 1.0;
    
    // Generate texture coordinates
    output.tex.x = (float) (input.vertexId & 1);
    output.tex.y = (float) (input.vertexId >> 1);
    
    return output;
}