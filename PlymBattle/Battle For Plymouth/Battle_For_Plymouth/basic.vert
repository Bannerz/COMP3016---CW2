#version 410 core

layout(location = 0) in vec3 aPosition;

uniform float uYaw;

out vec3 vPosition;

void main()
{
    float yaw = uYaw;
    float pitch = radians(20.0);

    //turn around y. x and z change
    vec3 turned = vec3(
        cos(yaw) * aPosition.x + sin(yaw) * aPosition.z,
        aPosition.y,
        -sin(yaw) * aPosition.x * cos(yaw) * aPosition.z
    );

    //tilt around x
    vec3 tilted = vec3(
        turned.x,
        cos(pitch) * turned.y - sin(pitch) * turned.z,
        sin(pitch) * turned.y + cos(pitch) * turned.z
    );

    vPosition = tilted;

    gl_Position = vec4(tilted, 1.0);
}