#version 410 core

//from FixedFuncShader.fxh 
#define FOG_TYPE_NONE            0 
#define FOG_TYPE_EXP             1 
#define FOG_TYPE_EXP2            2 
#define FOG_TYPE_LINEAR          3 

layout(std140) uniform FogConstantBuffer
{
  //fog settings 
  int iFogType; 
  float fFogStart; 
  float fFogEnd; 
  float fFogDensity; 
  vec4 vFogColor; 
};


// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
  vec4 pos;
  vec4 color;
  float  FogFactor;
};

// A pass-through function for the (interpolated) color data.
layout(location = 0) out vec4 _psout_main;
layout(location = 0) in vec4 _in_input_color;
layout(location = 1) in float _in_input_FogFactor;



void main()
{
  PixelShaderInput input;
  input.color = _in_input_color;
  input.FogFactor = _in_input_FogFactor;

  if ( input.color.a < ( 8.0 / 255.0 ) )
  {
    discard;
  }

  input.color.rgb = mix( input.color.rgb, vFogColor.rgb, input.FogFactor );

  _psout_main = input.color;
}