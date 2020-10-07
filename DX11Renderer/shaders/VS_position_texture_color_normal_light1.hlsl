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

struct Material
{
  float4    MaterialEmissive;
  float4    MaterialAmbient;
  float4    MaterialDiffuse;
  float4    MaterialSpecular;
  float     MaterialSpecularPower;
  float3    MaterialPadding;
}; 

cbuffer MaterialConstantBuffer : register( b2 )
{
  Material  _Material;
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

  // Calculate the normal vector against the world matrix only.
  //set required lighting vectors for interpolation
  float3 normal = normalize( input.normal );
  normal = mul( normal, ( float3x3 )model );
  normal = mul( normal, ( float3x3 )view );
  normal = normalize( normal );

  float4  totalAmbient = Ambient;
  float4  totalDiffuse = float4( 0, 0, 0, 0 );
  float4  specularEffect = float4( 0, 0, 0, 0 );
  

  for ( int i = 0; i < 1; ++i )
  {
    // Invert the light direction for calculations.
    float3 L = mul( ( float3x3 )viewIT, -normalize( Light[i].Direction ) );
    float NdotL = dot( normal, L );
    totalAmbient += Light[i].Ambient;
    
    if ( NdotL > 0.f )
    {
      //compute diffuse color
      totalDiffuse += NdotL * Light[i].Diffuse;

      //add specular component
      /*
      if ( bSpecular )
      {
        float3 H = normalize( L + V );   //half vector
          Out.ColorSpec = pow( max( 0, dot( H, N ) ), fMaterialPower ) * lights[i].vSpecular;
      }*/
    }
  }
  totalAmbient *= _Material.MaterialAmbient;
  totalDiffuse *= _Material.MaterialDiffuse;

  specularEffect *= _Material.MaterialSpecular;

  float4  totalFactor = totalAmbient + totalDiffuse + specularEffect;
  
  //apply fog    
  #include "Include_VS_Fog.inc"

  output.color = input.color * totalFactor;
  //saturate
  output.color = min( 1, output.color );
  
  return output;
}
