#include "DefineFog.inc"

// A constant buffer that stores the three basic column-major matrices for composing geometry.
cbuffer ModelViewProjectionConstantBuffer : register( b0 )
{
  matrix model;
  matrix view;
  matrix viewIT;
  matrix projection;
  matrix ortho2d;
  matrix textureTransform;
};

struct DirectionLight
{
  float3    Direction;
  float     PaddingL1;
  float4    Ambient;
  float4    Diffuse;
  float4    Specular;
};

cbuffer LightsConstantBuffer : register( b1 )
{
  float4          Ambient;
  float3          EyePos;
  float           PaddingLC1;
  DirectionLight  Light[8];
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
  float4 color : COLOR0;
  float  FogFactor : TEXCOORD1;
};

// Simple shader to do vertex processing on the GPU.
PixelShaderInput main(VertexShaderInput input)
{
  PixelShaderInput output;

  float4 pos = float4( input.pos, 1.0f );
  output.pos = mul( pos, ortho2d );

  // Calculate the normal vector against the world matrix only.
  //set required lighting vectors for interpolation
  float3 normal = mul( input.normal, ( float3x3 )model );
  normal = normalize( normal );

  output.color = input.color + Ambient;

  #include "Include_VS_Fog_RHW.inc"

  return output;
}
