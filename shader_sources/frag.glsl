#version 330 core

in vec3  FragPos;
in vec3  Normal;
in vec2  TexCoord;

out vec4 FragColor;

uniform vec3    uViewPos;
uniform vec3    uLightColor;
uniform vec3    uObjectColor;   // tint (you can leave at 1.0,1.0,1.0)
uniform vec3    uAmbientColor;
uniform vec3    uLightDir;

uniform sampler2D uAlbedo;
uniform sampler2D uNormalMap;
uniform sampler2D uRoughMap;   // <<--- Now using roughness, not specular!
uniform float     uShininess;

void main() {
    // --- fetch textures ---
    vec3 albedo    = texture(uAlbedo,    TexCoord).rgb;
    vec3 normMap   = texture(uNormalMap, TexCoord).rgb * 2.0 - 1.0;
    float rough    = texture(uRoughMap,  TexCoord).r;

    // --- choose normal ---
    vec3 norm = length(normMap) > 0.0 
               ? normalize(normMap) 
               : normalize(Normal);

    // --- ambient ---
    vec3 ambient = uAmbientColor * albedo;

    // --- diffuse (directional light) ---
    vec3 L = normalize(-uLightDir);
    float diff = max(dot(norm, L), 0.0);
    vec3 diffuse = diff * uLightColor * albedo;

    // --- specular ---
    vec3 V = normalize(uViewPos - FragPos);
    vec3 R = reflect(-L, norm);

    // Use "shininess" reduced by roughness for a rougher surface, and reduce strength
    float specStrength = 1.0 - rough;  // (roughness: 0 = full shine, 1 = matte)
    float spec = pow(max(dot(V, R), 0.0), mix(uShininess, 1.0, rough));
    vec3 specular = spec * uLightColor * specStrength;

    // --- final color ---
    vec3 color = (ambient + diffuse + specular) * uObjectColor;
    FragColor = vec4(color, 1.0);
}
