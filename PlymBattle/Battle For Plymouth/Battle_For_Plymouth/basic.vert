#version 410 core

layout(location = 0) in vec3 aPosition;

uniform float uYaw;

uniform float uAspect;

uniform float uCameraZ;

out vec3 vPosition;

layout(location = 1) in vec2 aUV;
out vec2 vUV;

void main()
{
    float yaw = uYaw;
    float pitch = radians(20.0);

    //turn around y. x and z change
    vec3 turned = vec3(
        cos(yaw) * aPosition.x + sin(yaw) * aPosition.z,
        aPosition.y,
        -sin(yaw) * aPosition.x + cos(yaw) * aPosition.z
    );

    //tilt around x
    vec3 tilted = vec3(
        turned.x,
        cos(pitch) * turned.y - sin(pitch) * turned.z,
        sin(pitch) * turned.y + cos(pitch) * turned.z
    );

    vPosition = tilted;
    vUV = aUV;

    //express the vertex relative to the camera
    vec3 viewPosition = tilted - vec3(0.0,0.0, uCameraZ);

    float nearPlane = 0.1;
    float farPlane = 100.0;
    float focalScale = 1.0 / tan(radians(60.0) * 0.5);

    gl_Position = vec4(
        viewPosition.x * focalScale / uAspect,
        viewPosition.y * focalScale,
        -(farPlane + nearPlane) / (farPlane - nearPlane) * viewPosition.z - (2.0 * farPlane * nearPlane) / (farPlane - nearPlane),
        -viewPosition.z
    );
}