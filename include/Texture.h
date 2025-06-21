// Texture.h
#pragma once

#include <string>
#include <GL/glew.h>

class Texture {
public:
    // Load texture from file (stb_image)
    explicit Texture(const std::string& path, bool flipVertically = true);
    ~Texture();

    // Bind to texture unit
    void bind(GLenum unit = GL_TEXTURE0) const;

private:
    GLuint id = 0;
};