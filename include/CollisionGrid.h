// include/CollisionGrid.h
#pragma once
#include <glm/glm.hpp>
#include <vector>

struct AABB {
    glm::vec3 center;
    float halfSize;
    bool intersectsSphere(const glm::vec3& pos, float radius) const {
        glm::vec3 diff = glm::abs(pos - center);
        return (diff.x <= halfSize + radius) &&
               (diff.y <= halfSize + radius) &&
               (diff.z <= halfSize + radius);
    }
};

class CollisionGrid {
public:
    // Build from wall centers (assumes unit cubes at Y=0)
    void build(const std::vector<glm::vec3>& wallPositions, float wallHeight) {
        aabbs.clear();
        for (auto& wp : wallPositions) {
            AABB box;
            box.center = glm::vec3(wp.x, wallHeight * 0.5f, wp.z);
            box.halfSize = std::max(0.5f, wallHeight * 0.5f);
            aabbs.push_back(box);
        }
    }

    // Check sphere against all precomputed AABBs
    bool collides(const glm::vec3& pos, float radius) const {
        for (auto& box : aabbs) {
            if (box.intersectsSphere(pos, radius)) return true;
        }
        return false;
    }

private:
    std::vector<AABB> aabbs;
};
