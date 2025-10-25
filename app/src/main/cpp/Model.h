#ifndef ANDROIDGLINVESTIGATIONS_MODEL_H
#define ANDROIDGLINVESTIGATIONS_MODEL_H

#include <vector>
#include "TextureAsset.h"
#include <glm/glm.hpp>


struct Vertex {
    constexpr Vertex(const glm::vec3 &inPosition, const glm::vec2 &inUv) :
            position(inPosition), uv(inUv) {}

    glm::vec3 position;
    glm::vec2 uv;
};

typedef uint16_t Index;

class Model {
public:
    inline Model(
            std::vector<Vertex> vertices,
            std::vector<Index> indices,
            GLint VBO, GLint EBO
    )
            : vertices_(std::move(vertices)),
              indices_(std::move(indices)),
              vbo_(VBO), ebo_(EBO) {}

    inline Model(
            std::vector<Vertex> vertices,
            std::vector<Index> indices)
            : vertices_(std::move(vertices)),
              indices_(std::move(indices)) {}

    inline const Vertex *getVertexData() const {
        return vertices_.data();
    }

    inline const size_t getIndexCount() const {
        return indices_.size();
    }

    inline const Index *getIndexData() const {
        return indices_.data();
    }

    inline const GLint getVBO() const {
        return vbo_;
    }

    inline const GLint getEBO() const {
        return ebo_;
    }

private:
    std::vector<Vertex> vertices_;
    std::vector<Index> indices_;

    GLint vbo_{-1}, ebo_{-1};

};

#endif //ANDROIDGLINVESTIGATIONS_MODEL_H