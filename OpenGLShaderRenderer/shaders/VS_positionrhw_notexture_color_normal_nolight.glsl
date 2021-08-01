//from FixedFuncShader.fxh 
#define FOG_TYPE_NONE            0 
#define FOG_TYPE_EXP             1 
#define FOG_TYPE_EXP2            2 
#define FOG_TYPE_LINEAR          3 

uniform FogConstantBuffer : register( b3 )
{
  //fog settings 
  int iFogType      = FOG_TYPE_NONE; 
  float fFogStart   = 10.f; 
  float fFogEnd     = 25.f; 
  float fFogDensity = .02f; 
  float4 vFogColor  = float4(0.0f, 0.0f, 0.0f, 0.0f); 
};


// A constant buffer that stores the three basic column-major matrices for composing geometry.
uniform ModelViewProjectionConstantBuffer : register(b0)
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
  float3 pos;
  float3 normal;
  float4 color;
  float2 tex;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
  float4 pos;
  float4 color;
  float  FogFactor;
};

// Simple shader to do vertex processing on the GPU.
layout(location = 0) in float3 _vsin_input_pos;
layout(location = 1) in float3 _vsin_input_normal;
layout(location = 2) in float4 _vsin_input_color;
layout(location = 3) in float2 _vsin_input_tex;
layout(location = 0) out float4 _vsout_main_color;
layout(location = 1) out float _vsout_main_FogFactor;

#define _RETURN_(_RET_VAL_){\
_SET_GL_POSITION(_RET_VAL_.pos);\
_vsout_main_color = _RET_VAL_.color;\
_vsout_main_FogFactor = _RET_VAL_.FogFactor;\
return;}

void main()
{
    VertexShaderInput input;
    input.pos = _vsin_input_pos;
    input.normal = _vsin_input_normal;
    input.color = _vsin_input_color;
    input.tex = _vsin_input_tex;

  PixelShaderInput output;

  float4 pos = float4( input.pos, 1.0 );

  // Transform the vertex position into projected space.
  output.pos = mul( pos, ortho2d );
  
  output.FogFactor = 1. * ( iFogType != FOG_TYPE_NONE );


  // Pass the color through without modification.
  output.color = input.color;

  _RETURN_( output)
}