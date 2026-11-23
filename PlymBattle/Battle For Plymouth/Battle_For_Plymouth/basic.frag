#version 410 core

in vec3 vPosition;
out vec4 fragColor;

in vec2 vUV;
uniform sampler2D uBaseColor;
uniform bool uUseTexture;

void main()
{
    vec3 normal = normalize(cross(dFdx(vPosition), dFdy(vPosition)));
    vec3 lightDirection = normalize(vec3(-0.5, 0.7, 1.0));

    float sunlight = max(dot(normal, lightDirection), 0.0);
    float brightness = 0.35 + 0.65 * sunlight;

    vec3 baseColor = uUseTexture
        ? texture(uBaseColor, vUV).rgb
        : vec3(1.0, 0.55, 0.15);

    fragColor = vec4(baseColor * brightness, 1.0);
}