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


struct DirectionLight
{
  float3   Direction;
  float4    Ambient;
  float4    Diffuse;
  float4    Specular;
};

uniform LightsConstantBuffer : register( b1 )
{
  float4        Ambient;
  float3       EyePos;
  DirectionLight    Light[8];
  float           Padding;
};

uniform MaterialConstantBuffer : register( b2 )
{
  float4    MaterialEmissive;
  float4    MaterialAmbient;
  float4    MaterialDiffuse;
  float4    MaterialSpecular;
  float     MaterialSpecularPower;
  float3    MaterialPadding;
};

uniform sampler2D shaderTexture;
SamplerState SampleType;


// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
  float4 pos;
  float3 normal;
  float4 color;
  float  FogFactor;
};

// A pass-through function for the (interpolated) color data.
layout(location = 0) out float4 _psout_main;
layout(location = 0) in float3 _in_input_normal;
layout(location = 1) in float4 _in_input_color;
layout(location = 2) in float _in_input_FogFactor;

#define _RETURN_(_RET_VAL_){\
_psout_main = _RET_VAL_;\
return;}

void main()
{
    PixelShaderInput input;
    _GET_GL_FRAG_COORD(input.pos);
    input.normal = _in_input_normal;
    input.color = _in_input_color;
    input.FogFactor = _in_input_FogFactor;

  // Sample the pixel color from the texture using the sampler at this texture coordinate location.
  //float4 textureColor = shaderTexture.Sample( SampleType, input.tex ) *input.color;
  float4 textureColor = input.color;

  textureColor.rgb = lerp( textureColor.rgb, vFogColor.rgb, input.FogFactor );

  _RETURN_( textureColor)
}