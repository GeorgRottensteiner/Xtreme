#include "DefineFog.inc"

Texture2D shaderTexture;
SamplerState SampleType;


// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
  float4 pos : SV_POSITION;
  float2 tex : TEXCOORD0;
  float4 color : COLOR0;
  float  FogFactor : TEXCOORD1;
};

// A pass-through function for the (interpolated) color data.
float4 main(PixelShaderInput input) : SV_TARGET
{
  // Sample the pixel color from the texture using the sampler at this texture coordinate location.
  float4 textureColor = shaderTexture.Sample( SampleType, input.tex ) * input.color;
  
  textureColor.rgb = lerp( textureColor.rgb, vFogColor.rgb, input.FogFactor );
  
  return textureColor;
}
