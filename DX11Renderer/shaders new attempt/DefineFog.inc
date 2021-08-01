//from FixedFuncShader.fxh 
#define FOG_TYPE_NONE            0 
#define FOG_TYPE_EXP             1 
#define FOG_TYPE_EXP2            2 
#define FOG_TYPE_LINEAR          3 

cbuffer FogConstantBuffer : register( b3 )
{
  //fog settings 
  int iFogType      = FOG_TYPE_NONE; 
  float fFogStart   = 10.f; 
  float fFogEnd     = 25.f; 
  float fFogDensity = .02f; 
  float4 vFogColor  = float4(0.0f, 0.0f, 0.0f, 0.0f); 
};
