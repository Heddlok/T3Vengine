#pragma once

#include <GL/glew.h>    // Provides GLuint, glBindVertexArray, glDrawArrays, etc.
#include <cstddef>      // For std::size_t
#include <string>       // For std::string
#include <glm/glm.hpp>  // For glm::vec3 etc.

// Simple mesh wrapper that loads an OBJ file and stores interleaved vertex data
struct Mesh {
    GLuint        VAO = 0;
    GLuint        VBO = 0;
    std::size_t   vertexCount = 0;  // Number of vertices (triangles * 3)

    // Load the mesh from an OBJ file
    Mesh(const std::string& objPath);

    // Draw the mesh (binds VAO and issues draw call)
    void draw() const {
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexCount));
    }

    // Cleanup GPU resources
    ~Mesh();
};