#pragma once

#include <string>
#include <GL/glew.h>

class Material {
public:
    Material(const std::string& albedo,
             const std::string& normal,
             const std::string& roughness,
             float shininess);

    void bind(GLuint program) const;
    ~Material();

private:
    GLuint albedoTex = 0;
    GLuint normalTex = 0;
    GLuint roughTex  = 0;
    float  shininess = 32.0f;
};
