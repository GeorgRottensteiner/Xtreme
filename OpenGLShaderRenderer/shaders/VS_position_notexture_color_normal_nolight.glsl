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

struct DirectionLight
{
  vec3    Direction;
  float   PaddingL1;
  vec4    Ambient;
  vec4    Diffuse;
  vec4    Specular;
};

layout(std140) uniform LightsConstantBuffer
{
  vec4            Ambient;
  vec3            EyePos;
  float           PaddingLC1;
  DirectionLight  Light[8];
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
layout(location = 0) out vec4 _vsout_main_color;
layout(location = 1) out float _vsout_main_FogFactor;



void main()
{
    VertexShaderInput input;
    input.pos = _vsin_input_pos;
    input.normal = _vsin_input_normal;
    input.color = _vsin_input_color;
    input.tex = _vsin_input_tex;

  PixelShaderInput output;

  vec4 pos = vec4( input.pos, 1.0 );

  // Transform the vertex position into projected space.
  pos = pos * model * view * projection;

  output.pos = pos;

  // Pass the color through without modification.
  output.color = clamp( input.color + Ambient, vec4( 0.0, 0.0, 0.0, 0.0 ), vec4( 1.0, 1.0, 1.0, 1.0 ) );

  // fog does not apply  
  output.color.rgb = vec3( 0, 0, 0 );
  
  // fog does nothing here
  vec4 P = vec4( input.pos, 1.0 ) * model;
  P = P * view;
  
  float  d = 0.0;
  d = P.z;
  float fog = 1. * when_eq( iFogType, FOG_TYPE_NONE )              
             + 1. / exp( d * fFogDensity )                 
             * when_eq( iFogType, FOG_TYPE_EXP )              
             + 1. / exp( pow( d * fFogDensity, 2 ) )                 
             * when_eq( iFogType, FOG_TYPE_EXP2 )              
             + clamp( ( fFogEnd - d ) / ( fFogEnd - fFogStart ), 0.0, 1.0 )
           * when_eq( iFogType, FOG_TYPE_LINEAR );   
           
  output.FogFactor = 1.0 - fog;             

    
  gl_Position = output.pos;
  _vsout_main_color = output.color;
  _vsout_main_FogFactor = output.FogFactor;
}