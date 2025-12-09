#version 320 es
precision mediump float;
out vec4 FragColor;
in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Normal;
//shadow mapping use
in vec4 FragPosLightSpace;
in vec2 ScreenCoords;
uniform vec3 lightPos;

// IBL
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;
// material parameters
uniform sampler2D albedoMap;
uniform bool useAlbedoMap;
uniform vec3 albedoValue; // 默认albedo参数

uniform sampler2D normalMap;
uniform bool useNormalMap;
uniform vec3 normalValue; // 默认normal参数

uniform sampler2D metallicMap;
uniform bool useMetallicMap;
uniform float metallicValue; // 默认metallic参数

uniform sampler2D roughnessMap;
uniform bool useRoughnessMap;
uniform float roughnessValue; // 默认roughness参数

uniform sampler2D aoMap;
uniform bool useAoMap;
uniform float aoValue; // 默认ao参数


uniform sampler2D shadowMap;
//uniform sampler2D ssao;
// lights
uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];

uniform vec3 camPos;

uniform bool lightChange;//光照改变

//shadow mapping use
float ShadowCalculation(vec4 PosLightSpace)
{
    // perform perspective divide
    vec3 projCoords = PosLightSpace.xyz / PosLightSpace.w;
    //vec3 projCoords = PosLightSpace.xyz ;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // calculate bias (based on depth map resolution and slope)
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(lightPos - WorldPos);
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    // check whether current frag pos is in shadow
    // float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0)
        shadow = 0.0;

    return shadow;
    //return 1.0;
}

const float PI = 3.14159265359;
// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
// Don't worry if you don't get what's going on; you generally want to do normal
// mapping the usual way for performance anyways; I do plan make a note of this
// technique somewhere later in the normal mapping tutorial.
vec3 getNormalFromMap()
{
//TODO: gyp:等待模型加载模块集成以后，取消"!"
    vec3 tangentNormal = useNormalMap? (texture(normalMap, TexCoords).xyz * 2.0 - 1.0) : vec3(0.0,0.0,1.0);

    vec3 Q1  = dFdx(WorldPos);
    vec3 Q2  = dFdy(WorldPos);
    vec2 st1 = dFdx(TexCoords);
    vec2 st2 = dFdy(TexCoords);

    vec3 N   = normalize(Normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------
void main()
{
    // material properties
//TODO: gyp:等待模型加载模块集成以后，取消"!"
    vec3  albedo = useAlbedoMap ? pow(texture(albedoMap, TexCoords).rgb, vec3(2.2)) : albedoValue;
    float metallic = useMetallicMap ? texture(metallicMap, TexCoords).r : metallicValue;
    float roughness = useRoughnessMap ? texture(roughnessMap, TexCoords).r : roughnessValue;
    //float ao = useAoMap ? texture(aoMap, TexCoords).r : aoValue;
    //roughness = 0.0f;

    //vec2 screenUV = gl_FragCoord.xy / vec2(textureSize(aoMap, 0));
    //改用ssao，不使用传入的useAoMap和aoValue
    float ao = texture(aoMap, ScreenCoords).r;

    //ssao
    //float ao = texture(ssao, TexCoords).r;

    // input lighting data
    vec3 N = getNormalFromMap();
    vec3 V = normalize(camPos - WorldPos);
    vec3 R = reflect(-V, N);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);
    for(int i = 0; i < 4; ++i)
    {
        // calculate per-light radiance
        vec3 L = normalize(lightPositions[i] - WorldPos);
        vec3 H = normalize(V + L);
        float distance = length(lightPositions[i] - WorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = lightColors[i] * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;

         // kS is equal to Fresnel
        vec3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);

        // add to outgoing radiance Lo
        Lo += (kD * albedo / PI + specular) * radiance * NdotL; // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    }

    // ambient lighting (we now use IBL as the ambient term)
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse      = irradiance * albedo;

    // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R,  roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    //vec3 ambient = (kD * diffuse + specular) * ao;
    vec3 ambient = (kD * diffuse + specular);

    //shadow mapping use
    // calculate shadow
    float shadow = ShadowCalculation(FragPosLightSpace);
    shadow = 0.0f;
    vec3 color = (1.0 - shadow * 0.5) * ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2));
    if(lightChange){
        color *= 0.6;
    }

    FragColor = vec4(color , 1.0);

    //vec3 testnormalMap = texture(normalMap, TexCoords).xyz;
    //FragColor = vec4(testnormalMap, 1.0); // for debugging normal map
    //FragColor = vec4(albedo, 1.0); // for debugging albedo map
    //FragColor = vec4(metallic,metallic,metallic, 1.0); // for debugging metallic value
     //FragColor = vec4(roughness,roughness,roughness, 1.0); // for debugging roughness value
     //vec3 testShadowMap = texture(shadowMap, TexCoords).xxx;
     //FragColor = vec4(testShadowMap, 1.0);
     //FragColor = vec4(normalize(Normal), 1.0);
     //FragColor = vec4(vec3(ao), 1.0);
     //FragColor = vec4(ScreenCoords,1.0, 1.0);
}