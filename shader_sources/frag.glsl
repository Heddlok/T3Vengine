#version 330 core
in  vec3 vNormal;
in  vec2 vUV;
out vec4 FragColor;

uniform mat4 uMVP;
uniform vec3 uLightDir;
uniform vec3 uCameraPos;

uniform sampler2D uAlbedo;      // binding=0
uniform sampler2D uNormalMap;   // binding=1
uniform sampler2D uSpecularMap; // binding=2
uniform float     uShininess;

void main() {
    // fetch maps
    vec3 albedo = texture(uAlbedo, vUV).rgb;
    vec3 normTex = texture(uNormalMap, vUV).rgb;
    vec3 N = normalize(normTex * 2.0 - 1.0);
    float specStrength = texture(uSpecularMap, vUV).r;

    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCameraPos - vec3(gl_FragCoord)); // or pass fragPos

    // diffuse
    float diff = max(dot(N, L), 0.0);

    // specular (Blinn-Phong)
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uShininess) * specStrength;

    vec3 color = albedo * diff + spec;
    FragColor = vec4(color, 1.0);
}
