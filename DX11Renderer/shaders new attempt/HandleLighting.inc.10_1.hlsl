struct Light
{
    float4      Position;               // 16 bytes
    //----------------------------------- (16 byte boundary)
    float4      Direction;              // 16 bytes
    //----------------------------------- (16 byte boundary)
    float4      Color;                  // 16 bytes
    //----------------------------------- (16 byte boundary)
    float4      Specular;               // 16 bytes
    //----------------------------------- (16 byte boundary)
    float4      Ambient;                // 16 bytes
    //----------------------------------- (16 byte boundary)
    float       SpotAngle;              // 4 bytes
    float       ConstantAttenuation;    // 4 bytes
    float       LinearAttenuation;      // 4 bytes
    float       QuadraticAttenuation;   // 4 bytes
    //----------------------------------- (16 byte boundary)
    int         LightType;              // 4 bytes
    bool        Enabled;                // 4 bytes
    int2        Padding;                // 8 bytes
    //----------------------------------- (16 byte boundary)
};  // Total:                           // 112 bytes (7 * 16 byte boundary)



cbuffer LightProperties : register(b1)
{
    float4 EyePosition;                 // 16 bytes
    //----------------------------------- (16 byte boundary)
    float4 GlobalAmbient;               // 16 bytes
    //----------------------------------- (16 byte boundary)
    Light Lights[MAX_LIGHTS];           // 80 * 8 = 640 bytes
};  // Total:                           // 672 bytes (42 * 16 byte boundary)




float4 DoDiffuse( Light light, float3 L, float3 N )
{
    float NdotL = max( 0, dot( N, L ) );
    return light.Color * NdotL;
}

float4 DoSpecular( Light light, float3 V, float3 L, float3 N )
{
    // Phong lighting.
    float3 R = normalize( reflect( -L, N ) );
    float RdotV = max( 0, dot( R, V ) );

    // Blinn-Phong lighting
    //float3 H = normalize( L + V );
    //float NdotH = max( 0, dot( N, H ) );

    return light.Specular * pow( RdotV, MaterialSpecularPower );
}

/*
float DoAttenuation( Light light, float d )
{
    return 1.0f / ( light.ConstantAttenuation + light.LinearAttenuation * d + light.QuadraticAttenuation * d * d );
}*/

struct LightingResult
{
    float4 Diffuse;
    float4 Specular;
};

/*
LightingResult DoPointLight( Light light, float3 V, float4 P, float3 N )
{
    LightingResult result;

    float3 L = ( light.Position - P ).xyz;
    float distance = length(L);
    L = L / distance;

    float attenuation = 1.0;//DoAttenuation( light, distance );

    result.Diffuse = DoDiffuse( light, L, N ) * attenuation;
    result.Specular = DoSpecular( light, V, L, N ) * attenuation;

    return result;
}
*/

LightingResult DoDirectionalLight( Light light, float3 V, float4 P, float3 N )
{
    LightingResult result;

    float3 L = -light.Direction.xyz;

    result.Diffuse = DoDiffuse( light, L, N );
    result.Specular = DoSpecular( light, V, L, N );

    return result;
}

/*
float DoSpotCone( Light light, float3 L )
{
    float minCos = cos( light.SpotAngle );
    float maxCos = ( minCos + 1.0f ) / 2.0f;
    float cosAngle = dot( light.Direction.xyz, -L );
    return smoothstep( minCos, maxCos, cosAngle ); 
}

LightingResult DoSpotLight( Light light, float3 V, float4 P, float3 N )
{
    LightingResult result;

    float3 L = ( light.Position - P ).xyz;
    float distance = length(L);
    L = L / distance;

    float attenuation = DoAttenuation( light, distance );
    float spotIntensity = DoSpotCone( light, L );

    result.Diffuse = DoDiffuse( light, L, N ) * attenuation * spotIntensity;
    result.Specular = DoSpecular( light, V, L, N ) * attenuation * spotIntensity;

    return result;
}
*/

LightingResult ComputeLighting( float4 P, float3 N )
{
    float3 V = normalize( EyePosition - P ).xyz;

    LightingResult totalResult = { {0, 0, 0, 0}, {0, 0, 0, 0} };

    [unroll]
    for( int i = 0; i < MAX_LIGHTS; ++i )
    {
        LightingResult result = { {0, 0, 0, 0}, {0, 0, 0, 0} };

        if ( !Lights[i].Enabled ) continue;

        //if ( Lights[i].LightType == DIRECTIONAL_LIGHT )
        //{
            result = DoDirectionalLight( Lights[i], V, P, N );
        //}
        //else if ( Lights[i].LightType == POINT_LIGHT )
        //{
            //result = DoPointLight( Lights[i], V, P, N );
        //}
        //else if ( Lights[i].LightType == SPOT_LIGHT )
        //{
            //result = DoSpotLight( Lights[i], V, P, N );
        //}
        totalResult.Diffuse += result.Diffuse;
        totalResult.Specular += result.Specular;
    }

    totalResult.Diffuse = saturate(totalResult.Diffuse);
    totalResult.Specular = saturate(totalResult.Specular);

    return totalResult;
}
