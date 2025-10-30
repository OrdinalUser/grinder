#version 330 core
layout (location = 0) in vec3 aPos; // The point is at (0,0,0) in local space

// Uniforms are global variables for a shader program
uniform mat4 modelMatrix;
uniform vec3 objectColor;

out vec3 vColor;

// We'll add a projection and view matrix later for a proper camera
uniform mat4 projection;
uniform mat4 view;

void main() {
    // For now, we ignore camera and just use the model matrix
    gl_Position = projection * view * modelMatrix * vec4(aPos, 1.0);
    gl_PointSize = 10.0; // Make the points a decent size
    vColor = objectColor;
}
