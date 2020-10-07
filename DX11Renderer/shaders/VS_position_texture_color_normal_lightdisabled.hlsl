#include "DefineFog.inc"

// A constant buffer that stores the three basic column-major matrices for composing geometry.
cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
  matrix model;
  matrix view;
  matrix viewIT;
  matrix projection;
  matrix ortho2d;
  matrix textureTransform;
};

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
  float3 pos : POSITION;
  float3 normal : NORMAL;
  float4 color : COLOR0;
  float2 tex : TEXCOORD0;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
  float4 pos : SV_POSITION;
  float2 tex : TEXCOORD0;
  float4 color : COLOR0;
  float  FogFactor : TEXCOORD1;
};

// Simple shader to do vertex processing on the GPU.
PixelShaderInput main(VertexShaderInput input)
{
  PixelShaderInput output;

  float4 pos = float4( input.pos, 1.0f );

  // Transform the vertex position into projected space.
  pos = mul(pos, model);
  pos = mul(pos, view);
  pos = mul(pos, projection);

  output.pos = pos;

  // pass texture coords
  //output.tex = input.tex;
  output.tex = mul( float4( input.tex, 0.0f, 1.0f ), textureTransform );

  //apply fog    
  #include "Include_VS_Fog.inc"

  // Pass the color through without modification.
  output.color = input.color;

  return output;
}
