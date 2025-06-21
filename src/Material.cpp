#include "Material.h"
#include "stb_image.h"

#include <GL/glew.h>
#include <filesystem>
#include <iostream>

// Default textures
static unsigned char checker[2*2*3] = {
    255,255,255,   0,0,0,
      0,0,0,     255,255,255
};
static unsigned char flatN[2*2*3] = {
    128,128,255,  128,128,255,
    128,128,255,  128,128,255
};
static unsigned char whiteR[2*2*3] = {
    255,255,255,  255,255,255,
    255,255,255,  255,255,255
};

static GLuint createDefaultTexture(int width, int height, const unsigned char* pixels, int channels) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLenum fmt = (channels == 4 ? GL_RGBA : GL_RGB);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, width, height, 0, fmt, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static GLuint loadTexture(const std::string& fullPath, const std::string& type) {
    if (fullPath.empty()) return 0;
    int w, h, n;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &n, 0);
    if (!data) {
        std::cerr << "Warning: failed to load " << type
                  << " texture at " << fullPath << "; using fallback.\n";

        if (type == "albedo")
            return createDefaultTexture(2, 2, checker, 3);
        if (type == "normal")
            return createDefaultTexture(2, 2, flatN, 3);
        if (type == "roughness")
            return createDefaultTexture(2, 2, whiteR, 3);
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLenum fmt = (n == 4 ? GL_RGBA : (n == 3 ? GL_RGB : GL_RED));
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    return tex;
}

Material::Material(const std::string& a,
                   const std::string& n,
                   const std::string& r,
                   float shin)
    : shininess(shin)
{
    std::string base = std::filesystem::path(ASSET_DIR).string();
    albedoTex  = !a.empty() ? loadTexture(base + "/" + a, "albedo")    : createDefaultTexture(2, 2, checker, 3);
    normalTex  = !n.empty() ? loadTexture(base + "/" + n, "normal")    : createDefaultTexture(2, 2, flatN, 3);
    roughTex   = !r.empty() ? loadTexture(base + "/" + r, "roughness") : createDefaultTexture(2, 2, whiteR, 3);
}

void Material::bind(GLuint program) const {
    // Albedo
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedoTex);
    glUniform1i(glGetUniformLocation(program, "uAlbedo"), 0);

    // Normal
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTex);
    glUniform1i(glGetUniformLocation(program, "uNormalMap"), 1);

    // Roughness
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, roughTex);
    glUniform1i(glGetUniformLocation(program, "uRoughMap"), 2);

    // Shininess
    glUniform1f(glGetUniformLocation(program, "uShininess"), shininess);
}

Material::~Material() {
    if (albedoTex)   glDeleteTextures(1, &albedoTex);
    if (normalTex)   glDeleteTextures(1, &normalTex);
    if (roughTex)    glDeleteTextures(1, &roughTex);
}
