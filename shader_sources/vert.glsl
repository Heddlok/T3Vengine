#version 330 core

// Vertex attributes
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;

// Uniforms
uniform mat4 uMVP;           // Model‐View‐Projection matrix
uniform mat4 uModel;         // Model matrix
uniform mat4 uView;          // View matrix

// Outputs to fragment shader
out vec2 vUV;
out vec3 vNormal;            // In world space
out vec3 vFragPos;           // World‐space position

void main() {
    // Pass UV coordinates straight through
    vUV = aUV;

    // Compute world‐space position
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;

    // Transform normal into world space
    // Note: for non‐uniform scales you'd compute the inverse‐transpose of uModel
    vNormal = normalize(mat3(uModel) * aNormal);

    // Final clip‐space position
    gl_Position = uMVP * vec4(aPos, 1.0);
}
