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

// Varyings to fragment shader
out vec4    _psin_color;
out float   _psin_fogfactor;


float when_eq(int x, int y) {
    return 1.0 - abs(sign(float(x - y)));
}


void main()
{ 
  vec4 pos = vec4( _vsin_input_pos, 1.0 );

  gl_Position = ortho2d * pos;
  
  // FogFactor = 1 - (iFogType == NONE ? 1 : 0)
  _psin_fogfactor = 1.0 - when_eq(iFogType, FOG_TYPE_NONE);
    
  _psin_color = _vsin_input_color;
}
