#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <GL/glew.h>
#include <stdexcept>
#include <filesystem>
#include <iostream>

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

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    // Load OBJ file
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objPath.c_str())) {
        throw std::runtime_error("Failed to load OBJ '" + objPath + "': " + warn + err);
    }

    // Extract vertex data (pos, normal, uv)
    std::vector<float> data;
    for (const auto& shape : shapes) {
        for (const auto& idx : shape.mesh.indices) {
            // Position
            data.push_back(attrib.vertices[3 * idx.vertex_index + 0]);
            data.push_back(attrib.vertices[3 * idx.vertex_index + 1]);
            data.push_back(attrib.vertices[3 * idx.vertex_index + 2]);
            // Normal (floats 3-5)
            if (!attrib.normals.empty()) {
                data.push_back(attrib.normals[3 * idx.normal_index + 0]);
                data.push_back(attrib.normals[3 * idx.normal_index + 1]);
                data.push_back(attrib.normals[3 * idx.normal_index + 2]);
            } else {
                data.push_back(0.0f);
                data.push_back(0.0f);
                data.push_back(0.0f);
            }
            // UV (floats 6-7)
            if (!attrib.texcoords.empty()) {
                data.push_back(attrib.texcoords[2 * idx.texcoord_index + 0]);
                data.push_back(attrib.texcoords[2 * idx.texcoord_index + 1]);
            } else {
                data.push_back(0.0f);
                data.push_back(0.0f);
            }
        }
    }

    vertexCount = data.size() / 8; // 8 floats per vertex
    std::cout << "Extracted " << vertexCount << " vertices from OBJ." << std::endl;

    if (vertexCount == 0) {
        std::cerr << "WARNING: No vertices were extracted from the OBJ file." << std::endl;
    }

    // Generate and bind VAO/VBO
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

    // Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // UV attribute (location = 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Normal attribute (location = 2)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

Mesh::~Mesh() {
    if (VBO) glDeleteBuffers(1, &VBO);
    if (VAO) glDeleteVertexArrays(1, &VAO);
}
