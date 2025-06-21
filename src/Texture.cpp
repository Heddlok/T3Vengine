// Texture.cpp
#include "Texture.h"
#include <stb_image.h>
#include <stdexcept>
#include <iostream>

Texture::Texture(const std::string& path, bool flipVertically) {
    stbi_set_flip_vertically_on_load(flipVertically);
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        throw std::runtime_error("Failed to load texture: " + path);
    }

    GLenum format = GL_RGB;
    if (channels == 1)      format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Default filtering/wrapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
}

Texture::~Texture() {
    if (id) glDeleteTextures(1, &id);
}

void Texture::bind(GLenum unit) const {
    glActiveTexture(unit);
    glBindTexture(GL_TEXTURE_2D, id);
}