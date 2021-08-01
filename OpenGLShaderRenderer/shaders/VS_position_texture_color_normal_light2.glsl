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

struct Material
{
  vec4    MaterialEmissive;
  vec4    MaterialAmbient;
  vec4    MaterialDiffuse;
  vec4    MaterialSpecular;
  float     MaterialSpecularPower;
  //vec3    MaterialPadding;
}; 

layout(std140) uniform MaterialConstantBuffer
{
  Material  _Material;
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

  // Transform the vertex position into projected space.
  pos = pos * model * view * projection;

  output.pos = pos;

  // pass texture coords
  output.tex = ( vec4( input.tex, 0.0, 1.0 ) * textureTransform ).xy;

  // transpose of inverse of upper/left 3x3
  mat3x3  matN = transpose( inverse( mat3x3( model ) * mat3x3( view ) ) );

  //set required lighting vectors for interpolation
  vec3 normal = normalize( input.normal );
  normal *= mat3x3( model );
  normal *= mat3x3( view );
  normal = normalize( normal );

  // GL Shader
  //vec3 normal = matN * input.normal;
  //normal = normalize( normal );

  //DX11
  //float3 normal = normalize( input.normal );
  //normal = mul( normal, ( float3x3 )model );
  //normal = mul( normal, ( float3x3 )view );
  //normal = normalize( normal );

  vec4  totalAmbient = Ambient;
  vec4  totalDiffuse = vec4( 0, 0, 0, 0 );
  vec4  specularEffect = vec4( 0, 0, 0, 0 );
  

  for ( int i = 0; i < 2; ++i )
  {
    // Invert the light direction for calculations.

    // GL Shader
    //vec3 L = mat3x3( viewIT ) * ( normalize( Light[i].Direction ) );
    //float NdotL = max( dot( normal, L ), 0 );


    vec3 L = mat3x3( viewIT ) * ( -normalize( Light[i].Direction ) );
    float NdotL = dot( normal, L );

    totalAmbient += Light[i].Ambient;

    
    //compute diffuse color
    totalDiffuse += NdotL * Light[i].Diffuse * _Material.MaterialDiffuse;

    //add specular component
    /*
    if ( bSpecular )
    {
    vec3 H = normalize( L + V );   //half vector
        Out.ColorSpec = pow( max( 0, dot( H, N ) ), fMaterialPower ) * lights[i].vSpecular;
    }*/
  }
  totalAmbient *= _Material.MaterialAmbient;
  //totalDiffuse *= 3;

  specularEffect *= _Material.MaterialSpecular;

  //vec4  totalFactor = totalAmbient + totalDiffuse + specularEffect;
  vec4  totalFactor = totalDiffuse;
  
  //apply fog    
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

  output.color = input.color * totalFactor;


  //saturate
  output.color = clamp( output.color, vec4( 0.0, 0.0, 0.0, 0.0), vec4( 1.0, 1.0, 1.0, 1.0) );
  
  gl_Position = output.pos;
  _vsout_main_tex = output.tex;
  _vsout_main_color = output.color;
  _vsout_main_FogFactor = output.FogFactor;
}