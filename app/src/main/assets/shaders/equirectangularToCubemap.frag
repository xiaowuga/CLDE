#version 320 es
precision mediump float;
out vec4 FragColor;
in vec3 WorldPos;

uniform sampler2D equirectangularMap;
uniform sampler2D equirectangularMap1;  // 第二个HDR贴图
uniform int hdrTextureIndex;            // 选择使用哪个贴图 (0 或 1)

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    vec2 uv = SampleSphericalMap(normalize(WorldPos));
    vec3 color;
    // vec3 color = texture(equirectangularMap, uv).rgb;

    // 根据参数选择使用哪个HDR贴图
    if (hdrTextureIndex == 0) {
        color = texture(equirectangularMap, uv).rgb;
    } else {
        color = texture(equirectangularMap1, uv).rgb;
    }

    FragColor = vec4(color, 1.0);
}