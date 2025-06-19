#pragma once

#include <string>
#include <GL/glew.h>

struct Material {
    GLuint albedoTex   = 0;
    GLuint normalTex   = 0;
    GLuint specularTex = 0;
    float  shininess   = 32.0f;

    // Paths are relative to ASSET_DIR
    Material(const std::string& albedoPath,
             const std::string& normalPath,
             const std::string& specularPath,
             float shininess = 32.0f);

    // Bind textures to units 0/1/2 and set uniforms
    void bind(GLuint shaderProgram) const;
    ~Material();
};
