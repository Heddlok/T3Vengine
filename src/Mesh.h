#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <GL/glew.h>

class Mesh {
public:
    // Load a mesh from an OBJ file
    Mesh(const std::string& objPath);
    ~Mesh();

    // Draw without instancing (e.g., floor)
    void drawPlain();

    // Draw with instancing (e.g., walls)
    void drawInstanced(GLsizei instanceCount);

    // Upload per-instance model matrices
    void setInstanceBuffer(const std::vector<glm::mat4>& instanceData);

private:
    // VAO for non-instanced draws
    GLuint VAO_plain   = 0;
    // VAO for instanced draws
    GLuint VAO_inst    = 0;
    // Vertex buffer for mesh data (positions, normals)
    GLuint VBO         = 0;
    // Instance buffer for model matrices
    GLuint instanceVBO = 0;
    // Number of vertices (triangle count * 3)
    GLsizei vertexCount = 0;
};
