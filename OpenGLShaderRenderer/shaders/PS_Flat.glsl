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


// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
  float4 pos;
  float3 color;
  float  FogFactor;
};

// A pass-through function for the (interpolated) color data.
layout(location = 0) out float4 _psout_main;
layout(location = 0) in float3 _in_input_color;
layout(location = 1) in float _in_input_FogFactor;

#define _RETURN_(_RET_VAL_){\
_psout_main = _RET_VAL_;\
return;}

void main()
{
    PixelShaderInput input;
    _GET_GL_FRAG_COORD(input.pos);
    input.color = _in_input_color;
    input.FogFactor = _in_input_FogFactor;

  float3 resultColor = lerp( input.color, vFogColor.rgb, input.FogFactor );

  _RETURN_( float4( resultColor, 1.0))
}