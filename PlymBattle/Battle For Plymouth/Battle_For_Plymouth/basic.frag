#version 410 core

in vec3 vPosition;
out vec4 fragColor;

in vec2 vUV;
uniform sampler2D uBaseColor;
uniform bool uUseTexture;

in vec3 vMapColor;
uniform bool uMapPreview;
uniform vec3 uMapTint;

void main()
{
    //cross the position gradients to estimate a flat surface normal.
    vec3 normal = normalize(cross(dFdx(vPosition), dFdy(vPosition)));
    vec3 lightDirection = normalize(vec3(-0.5, 0.7, 1.0));

    float sunlight = max(dot(normal, lightDirection), 0.0);
    //leave some ambient brightness on surfaces facing away from the light.
    float brightness = 0.35 + 0.65 * sunlight;

    //the sword samples its UV texture when available. Orange is the fallback.
    vec3 baseColor = uUseTexture
        ? texture(uBaseColor, vUV).rgb
        : vec3(1.0, 0.55, 0.15);

    //map mode uses vertex colours multiplied by the material tint.
    if (uMapPreview) {
        baseColor = vMapColor * uMapTint;
    }

    fragColor = vec4(baseColor * brightness, 1.0);
}