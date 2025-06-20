#include "Map.h"
#include <fstream>
#include <iostream>

bool Map::load(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return false;
    grid.clear();
    zombieSpawns.clear();
    int y = 0;
    std::string line;
    while (std::getline(in, line)) {
        for (int x = 0; x < (int)line.size(); ++x) {
            if (line[x] == 'P') playerSpawn = {x, y};
            if (line[x] == 'Z') zombieSpawns.push_back({x, y});
        }
        grid.push_back(line);
        ++y;
    }
    return true;
}

bool Map::isWall(int x, int y) const {
    if (y < 0 || y >= (int)grid.size()) return false;
    if (x < 0 || x >= (int)grid[y].size()) return false;
    return grid[y][x] == '#';
}
