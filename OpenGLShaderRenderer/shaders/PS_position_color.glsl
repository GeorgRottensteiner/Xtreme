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


// A constant buffer that stores the three basic column-major matrices for composing geometry.
layout(std140) uniform ModelViewProjectionConstantBuffer
{
  mat4x4 model;
  mat4x4 view;
  mat4x4 viewIT;
  mat4x4 projection;
  mat4x4 ortho2d;
  mat4x4 textureTransform;
};

float when_eq(int x, int y) {
  return 1.0 - abs(sign(x - y));
}


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
  _psout_main = mix( input.color, vec4( vFogColor.rgb, 1.0 ), input.FogFactor );
}