#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <GL/glew.h>
#include <stdexcept>
#include <filesystem>
#include <iostream>
#include <vector>
#include <glm/glm.hpp>

#include "Mesh.h"

Mesh::Mesh(const std::string& objPath) {
    // Log the path and existence
    std::cout << "Trying to load OBJ at: " << objPath << std::endl;
    if (!std::filesystem::exists(objPath)) {
        std::cerr << "ERROR: OBJ file does not exist at the given path." << std::endl;
    } else {
        std::cout << "File exists, size: "
                  << std::filesystem::file_size(objPath)
                  << " bytes" << std::endl;
    }

    // --- load OBJ ---
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objPath.c_str())) {
        throw std::runtime_error("Failed to load OBJ: " + warn + err);
    }

    // flatten into interleaved array (pos, normal, uv)
    std::vector<float> data;
    for (auto& shape : shapes) {
        for (auto& idx : shape.mesh.indices) {
            // position
            data.push_back(attrib.vertices[3*idx.vertex_index+0]);
            data.push_back(attrib.vertices[3*idx.vertex_index+1]);
            data.push_back(attrib.vertices[3*idx.vertex_index+2]);
            // normal
            if (!attrib.normals.empty()) {
                data.push_back(attrib.normals[3*idx.normal_index+0]);
                data.push_back(attrib.normals[3*idx.normal_index+1]);
                data.push_back(attrib.normals[3*idx.normal_index+2]);
            } else {
                data.push_back(0); data.push_back(0); data.push_back(0);
            }
            // uv
            if (!attrib.texcoords.empty()) {
                data.push_back(attrib.texcoords[2*idx.texcoord_index+0]);
                data.push_back(attrib.texcoords[2*idx.texcoord_index+1]);
            } else {
                data.push_back(0); data.push_back(0);
            }
        }
    }
    vertexCount = static_cast<GLsizei>(data.size() / 8);
    std::cout << "Extracted " << vertexCount << " vertices from OBJ." << std::endl;

    // Create and fill VBO
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

    // --- PLAIN VAO ---
    glGenVertexArrays(1, &VAO_plain);
    glBindVertexArray(VAO_plain);
      glBindBuffer(GL_ARRAY_BUFFER, VBO);
      // pos @loc0
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
      glEnableVertexAttribArray(0);
      // normal @loc1
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
      glEnableVertexAttribArray(1);
      // uv @loc2
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
      glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // --- INSTANCED VAO ---
    glGenVertexArrays(1, &VAO_inst);
    glBindVertexArray(VAO_inst);
      glBindBuffer(GL_ARRAY_BUFFER, VBO);
      // pos @loc0
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
      glEnableVertexAttribArray(0);
      // normal @loc1
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
      glEnableVertexAttribArray(1);
      // uv @loc2
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
      glEnableVertexAttribArray(2);

      // instance matrix @loc3-6 (4 vec4 columns)
      glGenBuffers(1, &instanceVBO);
      glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
      // initially empty
      glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
      std::size_t vec4Size = sizeof(glm::vec4);
      for (int i = 0; i < 4; ++i) {
          GLuint loc = 3 + i;
          glEnableVertexAttribArray(loc);
          glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE,
                                sizeof(glm::mat4),
                                (void*)(i * vec4Size));
          glVertexAttribDivisor(loc, 1);
      }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Mesh::~Mesh() {
    if (instanceVBO) glDeleteBuffers(1, &instanceVBO);
    if (VBO)         glDeleteBuffers(1, &VBO);
    if (VAO_inst)    glDeleteVertexArrays(1, &VAO_inst);
    if (VAO_plain)   glDeleteVertexArrays(1, &VAO_plain);
}

void Mesh::setInstanceBuffer(const std::vector<glm::mat4>& instanceData) {
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 instanceData.size() * sizeof(glm::mat4),
                 instanceData.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::drawPlain() {
    glBindVertexArray(VAO_plain);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
}

void Mesh::drawInstanced(GLsizei instanceCount) {
    glBindVertexArray(VAO_inst);
    glDrawArraysInstanced(GL_TRIANGLES, 0, vertexCount, instanceCount);
    glBindVertexArray(0);
}
