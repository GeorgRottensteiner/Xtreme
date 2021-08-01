#include "DefineFog.inc"

struct DirectionLight
{
  float3   Direction;
  float4    Ambient;
  float4    Diffuse;
  float4    Specular;
};

cbuffer LightsConstantBuffer : register( b1 )
{
  float4        Ambient;
  float3       EyePos;
  DirectionLight    Light[8];
  float           Padding;
};

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
SamplerState SampleType;


// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
  float4 pos : SV_POSITION;
  float3 normal : NORMAL;
  float4 color : COLOR0;
  float  FogFactor : TEXCOORD1;
};

// A pass-through function for the (interpolated) color data.
float4 main(PixelShaderInput input) : SV_TARGET
{
  // Sample the pixel color from the texture using the sampler at this texture coordinate location.
  //float4 textureColor = shaderTexture.Sample( SampleType, input.tex ) *input.color;
  float4 textureColor = input.color;

  textureColor.rgb = lerp( textureColor.rgb, vFogColor.rgb, input.FogFactor );

  return textureColor;
}
