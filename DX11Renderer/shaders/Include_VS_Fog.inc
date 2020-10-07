  // fog does nothing here
  float4 P = mul( float4( input.pos, 1.0f ), model ); //position in view space 
  P = mul( P, view );
  
  float  d = 0.0f;
  //d = length( P );
  d = P.z;
  float fog = 1.f * ( iFogType == FOG_TYPE_NONE )              
             + 1.f / exp( d * fFogDensity )                 
             * ( iFogType == FOG_TYPE_EXP )              
             + 1.f / exp( pow( d * fFogDensity, 2 ) )                 
             * ( iFogType == FOG_TYPE_EXP2 )              
             + saturate( ( fFogEnd - d ) / ( fFogEnd - fFogStart ) )                 
           * ( iFogType == FOG_TYPE_LINEAR );   
           
  output.FogFactor = 1.0f - fog;           
  //output.FogFactor = fog;           
