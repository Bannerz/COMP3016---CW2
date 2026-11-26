#version 410 core

in vec3 vPosition;
out vec4 fragColor;

in vec2 vUV;
uniform sampler2D uBaseColor;
uniform bool uUseTexture;

in vec3 vMapColor;
uniform bool uMapPreview;
uniform vec3 uMapTint;
uniform int uMapTextureMode;

uniform bool uStaminaHud;
uniform float uStaminaFraction;

const int UV_TEXTURE = 1;
const int PROJECTED_TEXTURE = 2;
const int CANVAS = 3;
const int THATCH = 4;

// OpenGL decodes sRGB albedo samples to linear. Convert the lit result for the display.
vec3 displayColor(vec3 linearColor)
{
    vec3 c = clamp(linearColor, 0.0, 1.0);
    return mix(1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, 12.92 * c,
               lessThanEqual(c, vec3(0.0031308)));
}

float projectedDetail(vec3 normal)
{
    // Blend three planar samples so unwrapped walls, steps and roofs have no
    // stretched sides. World coordinates keep the pattern the same size across assets.
    vec3 p = vPosition * 0.65;
    vec3 weights = pow(abs(normal), vec3(6.0));
    weights /= max(dot(weights, vec3(1.0)), 0.0001);
    vec3 color = texture(uBaseColor, p.zy).rgb * weights.x
               + texture(uBaseColor, p.xz).rgb * weights.y
               + texture(uBaseColor, p.xy).rgb * weights.z;
    float lightness = sqrt(dot(color, vec3(0.2126, 0.7152, 0.0722)));
    return 0.62 + 0.76 * lightness;
}

float fibreDetail(vec3 normal, bool thatch)
{
    vec2 p = abs(normal.y) > 0.65 ? vPosition.xz :
        (abs(normal.x) > abs(normal.z) ? vPosition.zy : vPosition.xy);
    vec2 frequency = thatch ? vec2(145.0, 18.0) : vec2(125.0);
    // Fade out threads once they are too small to resolve at this distance.
    vec2 visible = 1.0 - smoothstep(vec2(1.0), vec2(3.14), fwidth(p) * frequency);
    float weft = sin(p.y * frequency.y);
    float warp = sin(p.x * frequency.x + (thatch ? weft * 0.45 : 0.0));
    return 1.0 + warp * visible.x * 0.13 + weft * visible.y * 0.06;
}

void main()
{

    if (uStaminaHud) {
        vec4 frame = texture(uBaseColor, vUV);
        vec2 pixel = vUV * vec2(1000.0, 119.0);
        float rightEdge = 884.0 - 0.7 * abs(pixel.y - 56.0);
        bool inside = pixel.x >= 155.0 && pixel.x <= rightEdge
                   && pixel.y >= 40.0 && pixel.y < 73.0;

        vec3 gold = displayColor(frame.rgb);
        if (inside) {
            float fillEdge = mix(155.0, 884.0, uStaminaFraction);
            vec3 green = vec3(0.24, 0.72, 0.28);
            vec3 empty = vec3(0.07, 0.10, 0.07);
            vec3 track = pixel.x < fillEdge ? green : empty;
            fragColor = vec4(mix(track, gold, frame.a), 1.0);
        } else {
            fragColor = vec4(gold, frame.a);
        }
        return;
    }

    // Cross the position gradients to estimate a flat surface normal.
    vec3 normal = normalize(cross(dFdx(vPosition), dFdy(vPosition)));
    vec3 lightDirection = normalize(vec3(-0.5, 0.7, 1.0));
    float sunlight = max(dot(normal, lightDirection), 0.0);
    float brightness = 0.35 + 0.65 * sunlight;

    if (uMapPreview) {
        vec3 tint = vMapColor * uMapTint;
        if (uMapTextureMode == UV_TEXTURE) {
            vec3 albedo = texture(uBaseColor, vUV).rgb * tint;
            fragColor = vec4(displayColor(albedo * brightness), 1.0);
            return;
        }

        float detail = 1.0;
        if (uMapTextureMode == PROJECTED_TEXTURE) {
            detail = projectedDetail(normal);
        }
        else if (uMapTextureMode == CANVAS || uMapTextureMode == THATCH) {
            detail = fibreDetail(normal, uMapTextureMode == THATCH);
        }
        fragColor = vec4(tint * brightness * detail, 1.0);
        return;
    }

    // The sword preview uses its own BMP and orange fallback.
    vec3 baseColor = uUseTexture ? texture(uBaseColor, vUV).rgb : vec3(1.0, 0.55, 0.15);
    fragColor = vec4(baseColor * brightness, 1.0);
}
