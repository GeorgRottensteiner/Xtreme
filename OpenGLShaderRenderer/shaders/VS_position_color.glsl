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
  vec4 color;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
  vec4 pos;
  vec4 color;
  float  FogFactor;
};

// Simple shader to do vertex processing on the GPU.
layout(location = 0) in vec3 _vsin_input_pos;
layout(location = 1) in vec4 _vsin_input_color;
layout(location = 0) out vec4 _vsout_main_color;
layout(location = 1) out float _vsout_main_FogFactor;

float when_eq(int x, int y) 
{
  return 1.0 - abs(sign(x - y));
}

void main()
{
    VertexShaderInput input;
    input.pos = _vsin_input_pos;
    input.color = _vsin_input_color;

  PixelShaderInput output;

  vec4 pos = vec4( input.pos, 1.0 );

  // Transform the vertex position into projected space.
  pos = pos * model * view * projection;

  output.pos = pos;

  //apply fog    
  // fog does nothing here
  //vec4 P = mul( vec4( input.pos, 1.0 ), model ); //position in view space 
  vec4 P = vec4( input.pos, 1.0 ) * model;
  //P = mul( P, view );
  P = P * view;
  
  float  d = 0.0;
  //d = length( P );
  d = P.z;
  float fog = 1. * when_eq( iFogType, FOG_TYPE_NONE )              
             + 1. / exp( d * fFogDensity )                 
             * when_eq( iFogType, FOG_TYPE_EXP )              
             + 1. / exp( pow( d * fFogDensity, 2 ) )                 
             * when_eq( iFogType, FOG_TYPE_EXP2 )              
             + clamp( ( fFogEnd - d ) / ( fFogEnd - fFogStart ), 0.0, 1.0 )
           * when_eq( iFogType, FOG_TYPE_LINEAR );     
           
  output.FogFactor = 1.0 - fog;           
  //output.FogFactor = fog;           


  // Pass the color through without modification.
  output.color = input.color;

  gl_Position = output.pos;
  _vsout_main_color = output.color;
  _vsout_main_FogFactor = output.FogFactor;
}