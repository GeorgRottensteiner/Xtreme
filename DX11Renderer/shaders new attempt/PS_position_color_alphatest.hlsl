#include "DefineFog.inc"

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
  float4 pos : SV_POSITION;
  float4 color : COLOR0;
  float  FogFactor : TEXCOORD1;
};

// A pass-through function for the (interpolated) color data.
float4 main(PixelShaderInput input) : SV_TARGET
{
  //clip( input.color.a <= ( 128.0f / 255.0f ) ? -1 : 1 );
  if ( input.color.a < ( 8.0f / 255.0f ) )
  {
    //input.color = float4( 1, 0, 0, 1 );
    discard;
  }
  input.color = lerp( input.color, vFogColor, input.FogFactor );

  return input.color;
}
