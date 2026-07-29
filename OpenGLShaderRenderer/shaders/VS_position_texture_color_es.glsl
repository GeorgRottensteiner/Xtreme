#version 300 es
precision mediump float;

// Fog types
#define FOG_TYPE_NONE   0
#define FOG_TYPE_EXP    1
#define FOG_TYPE_EXP2   2
#define FOG_TYPE_LINEAR 3

// Flattened fog uniforms
uniform int   iFogType;
uniform float fFogStart;
uniform float fFogEnd;
uniform float fFogDensity;
uniform vec4  vFogColor;

// Flattened matrix uniforms (WebGL1 has no UBOs)
uniform mat4 model;
uniform mat4 view;
uniform mat4 viewIT;
uniform mat4 projection;
uniform mat4 ortho2d;
uniform mat4 textureTransform;

// Attributes
in vec3 _vsin_input_pos;
in vec4 _vsin_input_color;
in vec2 _vsin_input_tex;
 
// Varyings to fragment shader
out vec4    _psin_color;
out vec2    _psin_tex;
out float   _psin_fogfactor;

float when_eq(int x, int y) 
{
  return 1.0 - abs(sign(float(x - y)));
}

void main()
{
  vec4 pos = vec4( _vsin_input_pos, 1.0 );

  // Transform the vertex position into projected space.
  gl_Position = pos * model * view * projection;

  //apply fog    
  // fog does nothing here
  vec4 P = vec4( _vsin_input_pos, 1.0 ) * model;
  P = P * view;
  
  float  d = 0.0;
  d = P.z;
  float fog = 1. * when_eq( iFogType, FOG_TYPE_NONE )              
             + 1. / exp( d * fFogDensity )                 
             * when_eq( iFogType, FOG_TYPE_EXP )              
             + 1. / exp( pow( d * fFogDensity, 2.0 ) )                 
             * when_eq( iFogType, FOG_TYPE_EXP2 )              
             + clamp( ( fFogEnd - d ) / ( fFogEnd - fFogStart ), 0.0, 1.0 )
           * when_eq( iFogType, FOG_TYPE_LINEAR );     
  
  _psin_color = _vsin_input_color;
  _psin_tex = _vsin_input_tex;
  _psin_fogfactor = 1.0 - fog;           
}