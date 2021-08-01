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

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
  vec3 pos;
  vec3 normal;
  vec4 color;
  vec2 tex;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
  vec4 pos;
  vec2 tex;
  vec4 color;
  float  FogFactor;
};

float when_eq(int x, int y) 
{
  return 1.0 - abs(sign(x - y));
}

// Simple shader to do vertex processing on the GPU.
layout(location = 0) in vec3 _vsin_input_pos;
layout(location = 1) in vec3 _vsin_input_normal;
layout(location = 2) in vec4 _vsin_input_color;
layout(location = 3) in vec2 _vsin_input_tex;
layout(location = 0) out vec2 _vsout_main_tex;
layout(location = 1) out vec4 _vsout_main_color;
layout(location = 2) out float _vsout_main_FogFactor;



void main()
{
  VertexShaderInput input;
  input.pos = _vsin_input_pos;
  input.normal = _vsin_input_normal;
  input.color = _vsin_input_color;
  input.tex = _vsin_input_tex;

  PixelShaderInput output;

  vec4 pos = vec4( input.pos, 1.0 );
  output.pos = pos * ortho2d;

  // pass texture coords
  output.tex = input.tex;
  
  output.FogFactor = 1.0 - when_eq(iFogType, FOG_TYPE_NONE);


  // Pass the color through without modification.
  output.color = input.color;

  gl_Position = output.pos;

  _vsout_main_tex = output.tex;
  _vsout_main_color = output.color;
  _vsout_main_FogFactor = output.FogFactor;
}