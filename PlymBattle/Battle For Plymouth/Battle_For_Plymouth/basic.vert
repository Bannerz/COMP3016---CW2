#version 410 core

//locations match the vertex attributes configured by the C++ upload code.
layout(location = 0) in vec3 aPosition;

uniform float uYaw;

uniform float uAspect;

uniform float uCameraZ;

uniform vec3 uMapEye;

uniform float uMapYaw;

uniform float uMapPitch;

out vec3 vPosition;

layout(location = 1) in vec2 aUV;
out vec2 vUV;

layout(location = 2) in vec3 aMapColor;
out vec3 vMapColor;

uniform bool uMapPreview;
uniform mat4 uModel;

void main()
{

    vMapColor = aMapColor;

    //map vertices go from mesh-local space to world space, then to camera space.
    if (uMapPreview) {
        vec3 world = (uModel * vec4(aPosition, 1.0)).xyz;

        vPosition = world;
        vUV = aUV;

       vec3 offset = world - uMapEye;

       //express the offset in the camera basis. Forward is negative Z in view space.
       float c = cos(uMapYaw);
       float s = sin(uMapYaw);
       vec3 view = vec3(
           c * offset.x + s * offset.z,
           offset.y,
          -s * offset.x + c * offset.z
       );

       //positive pitch looks down. Mouse input updates this angle in radians.
       float pitch = uMapPitch;
       view = vec3(view.x,
                   cos(pitch) * view.y - sin(pitch) * view.z,
                   sin(pitch) * view.y + cos(pitch) * view.z);

       float nearPlane = 0.5;
       float farPlane = 4500.0;
       float focalScale = 1.0 / tan(radians(60.0) * 0.5);

       //the perspective divide uses -view.z, so distant geometry appears smaller.
       gl_Position = vec4(
           view.x * focalScale / uAspect,
           view.y * focalScale,
           -(farPlane + nearPlane) / (farPlane - nearPlane) * view.z
               - (2.0 * farPlane * nearPlane) / (farPlane - nearPlane),
           -view.z
       );
        return;
    }

    float yaw = uYaw;
    float pitch = radians(20.0);

    //spin the sword around Y, leaving its vertical coordinate unchanged.
    vec3 turned = vec3(
        cos(yaw) * aPosition.x + sin(yaw) * aPosition.z,
        aPosition.y,
        -sin(yaw) * aPosition.x + cos(yaw) * aPosition.z
    );

    //apply a fixed tilt around X so the preview shows more than one side.
    vec3 tilted = vec3(
        turned.x,
        cos(pitch) * turned.y - sin(pitch) * turned.z,
        sin(pitch) * turned.y + cos(pitch) * turned.z
    );

    vPosition = tilted;
    vUV = aUV;

    //the sword camera translates along Z and uses a shorter viewing range.
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