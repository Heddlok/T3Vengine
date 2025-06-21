#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;        // planar UVs
layout(location = 3) in mat4 instanceModel;    // start at location 3

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform bool  uUseInstancing;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main() {
    mat4 model = uUseInstancing ? instanceModel : uModel;
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal  = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = worldPos.xz * 0.25;
    gl_Position = uProjection * uView * worldPos;
}
