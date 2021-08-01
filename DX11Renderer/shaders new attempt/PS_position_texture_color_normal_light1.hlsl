#define MAX_LIGHTS 2
// Light types.
#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

#include "DefineFog.inc"

cbuffer MaterialConstantBuffer : register( b2 )
{
  float4    MaterialEmissive;
  float4    MaterialAmbient;
  float4    MaterialDiffuse;
  float4    MaterialSpecular;
  float     MaterialSpecularPower;
  float3    MaterialPadding;
};

Texture2D shaderTexture;
SamplerState Sampler;



#include "HandleLighting.inc"




// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
  float4 PositionWS   : POS_WS;
  float3 NormalWS     : NORMAL_WS;
  float2 TexCoord     : TEXCOORD0;
  float4 Position     : SV_Position;
  float4 color        : COLOR0;
  float  FogFactor    : FOG_FACTOR;
};

float4 main( PixelShaderInput IN ) : SV_TARGET
{
    LightingResult lit = ComputeLighting( IN.PositionWS, normalize( IN.NormalWS ) );
    
    float4 emissive = MaterialEmissive;
    float4 ambient = MaterialAmbient * GlobalAmbient;
    float4 diffuse = MaterialDiffuse * lit.Diffuse;
    float4 specular = MaterialSpecular * lit.Specular;

    float4 texColor = IN.color;
    
    texColor *= shaderTexture.Sample( Sampler, IN.TexCoord );

    float4 finalColor = ( emissive + ambient + diffuse + specular ) * texColor;
    //float4 finalColor = texColor;

    finalColor.rgb = lerp( finalColor.rgb, vFogColor.rgb, IN.FogFactor );

    return finalColor;
}
