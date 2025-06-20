#pragma once

#include <GL/glew.h>
#include <cstddef>
#include <string>

struct Mesh {
    GLuint        VAO = 0;
    GLuint        VBO = 0;
    std::size_t   vertexCount = 0;

    Mesh(const std::string& objPath);
    ~Mesh();

    // Inline draw call
    void draw() const {
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexCount));
    }
};
