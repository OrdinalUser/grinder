#version 450 core

layout(location = 0) in vec3 aPos;        // quad: (-0.5,-0.5,0) ... 
layout(location = 1) in vec2 aTexCoord;

layout(location = 2) in vec3 iOffset;
layout(location = 3) in vec3 iVelocity;
layout(location = 4) in float iLife;      // 1.0 = fresh, 0.0 = dead

out vec2 vTexCoord;
out float vLife;
out float vSpeed; // for vertical stretching


uniform mat4 view;
uniform mat4 proj;

uniform float time;

void main()
{
    // Billboard: make the quad always face the camera
    vec3 worldPos = iOffset;

    // Camera right and up vectors (in world space)
    vec3 camRight = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camUp    = vec3(view[0][1], view[1][1], view[2][1]);

    // Scale the quad (thin vertical streak)
    float width  = 0.03;
    float height = 1.8 + length(iVelocity) * 0.07; // faster = longer streak

    vec3 vertexPos = worldPos
        + camRight * aPos.x * width
        + camUp    * aPos.y * height;

    // Slight upward offset based on texcoord so top fades nicely
    vertexPos += camUp * (aTexCoord.y - 0.5) * 0.3;

    gl_Position = proj * view * vec4(vertexPos, 1.0);

    vTexCoord = aTexCoord;
    vLife     = iLife;
    vSpeed    = length(iVelocity);
}