#include "DefineFog.inc"

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
  float4 pos : SV_POSITION;
  float3 color : COLOR0;
  float  FogFactor : TEXCOORD1;
};

// A pass-through function for the (interpolated) color data.
float4 main(PixelShaderInput input) : SV_TARGET
{
  float3 resultColor = lerp( input.color, vFogColor.rgb, input.FogFactor );

  return float4( resultColor, 1.0f);
}
