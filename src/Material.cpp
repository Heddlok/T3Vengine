#include "Material.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GL/glew.h>
#include <filesystem>
#include <iostream>

static GLuint createDefaultTexture(int width, int height, const unsigned char* pixels, int channels) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLenum fmt = (channels == 4 ? GL_RGBA : GL_RGB);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, width, height, 0, fmt, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

static GLuint loadTexture(const std::string& fullPath, const std::string& type) {
    int w, h, n;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &n, 0);
    if (!data) {
        std::cerr << "Warning: failed to load " << type << " texture at " << fullPath << "; using fallback.\n";
        // Fallback patterns:
        if (type == "albedo") {
            // 2x2 checkerboard
            static unsigned char checker[2*2*3] = {
                255,255,255,  0,0,0,
                  0,0,0,    255,255,255
            };
            return createDefaultTexture(2, 2, checker, 3);
        }
        if (type == "normal") {
            // 2x2 flat normal (128,128,255)
            static unsigned char flatNormal[2*2*3] = {
                128,128,255, 128,128,255,
                128,128,255, 128,128,255
            };
            return createDefaultTexture(2, 2, flatNormal, 3);
        }
        if (type == "specular") {
            // 2x2 white specular
            static unsigned char whiteSpec[2*2*3] = {
                255,255,255, 255,255,255,
                255,255,255, 255,255,255
            };
            return createDefaultTexture(2, 2, whiteSpec, 3);
        }
        return 0;
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLenum fmt = (n == 4 ? GL_RGBA : GL_RGB);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return tex;
}

Material::Material(const std::string& a, const std::string& n, const std::string& s, float shin)
    : shininess(shin)
{
    std::string base = std::filesystem::path(ASSET_DIR).string();
    albedoTex   = loadTexture(base + "/" + a, "albedo");
    normalTex   = loadTexture(base + "/" + n, "normal");
    specularTex = loadTexture(base + "/" + s, "specular");
}

void Material::bind(GLuint program) const {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedoTex);
    glUniform1i(glGetUniformLocation(program, "uAlbedo"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTex);
    glUniform1i(glGetUniformLocation(program, "uNormalMap"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, specularTex);
    glUniform1i(glGetUniformLocation(program, "uSpecularMap"), 2);

    glUniform1f(glGetUniformLocation(program, "uShininess"), shininess);
}

Material::~Material() {
    if (albedoTex)   glDeleteTextures(1, &albedoTex);
    if (normalTex)   glDeleteTextures(1, &normalTex);
    if (specularTex) glDeleteTextures(1, &specularTex);
}
