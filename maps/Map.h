#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>

struct Map {
    std::vector<std::string> grid;
    glm::ivec2 playerSpawn = {-1, -1};
    std::vector<glm::ivec2> zombieSpawns;

    bool load(const std::string& filename);
    bool isWall(int x, int y) const;
};
