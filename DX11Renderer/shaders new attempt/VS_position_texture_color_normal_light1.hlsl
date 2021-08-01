#include "DefineFog.inc"

// A constant buffer that stores the three basic column-major matrices for composing geometry.
cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
  matrix model;
  matrix view;
  matrix InverseTransposeWorldMatrix;
  matrix projection;
  matrix ortho2d;
  matrix textureTransform;
};

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
  float3 pos    : POSITION;
  float3 normal : NORMAL;
  float4 color  : COLOR0;
  float2 tex    : TEXCOORD0;
};

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

// Simple shader to do vertex processing on the GPU.
PixelShaderInput main(VertexShaderInput input)
{
 PixelShaderInput output;

  output.Position = mul( mul( mul( float4( input.pos, 1.0f ), model ), view ), projection );
  output.PositionWS = mul( model, float4( input.pos, 1.0f ) );
  //output.NormalWS = mul( (float3x3)InverseTransposeWorldMatrix, input.normal );
  output.NormalWS = normalize( mul( input.normal, model ) ); // mul( (float3x3)InverseTransposeWorldMatrix, input.normal );
  output.TexCoord = input.tex;
  output.color     = input.color;

  //apply fog    
  #include "Include_VS_Fog.inc"

  return output;
}
