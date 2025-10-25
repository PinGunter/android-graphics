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


struct Model {
    inline Model(
            std::vector<Vertex> vertices,
            std::vector<Index> indices
    )
            : vertices_(std::move(vertices)),
              indices_(std::move(indices)) {}

    std::vector<Vertex> vertices_;
    std::vector<Index> indices_;

};

class Drawable {
private:
    GLuint VAO{0}, VBO{0}, EBO{0};
    std::shared_ptr<Model> model;
public:
    Drawable(std::shared_ptr<Model> m) : model(m) {

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(
                GL_ARRAY_BUFFER,
                model->vertices_.size() * sizeof(Vertex),
                model->vertices_.data(),
                GL_STATIC_DRAW
        );

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                model->indices_.size() * sizeof(GLushort),
                model->indices_.data(),
                GL_STATIC_DRAW
        );

        GLsizei stride = sizeof(Vertex);

        glVertexAttribPointer(
                0,
                3, GL_FLOAT, GL_FALSE, stride,
                (const void *) offsetof(Vertex, position)
        );
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(
                1,
                3, GL_FLOAT, GL_FALSE, stride,
                (const void *) offsetof(Vertex, uv)
        );
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void draw() const {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, model->indices_.size(), GL_UNSIGNED_SHORT, 0);
        glBindVertexArray(0);
    };

    ~Drawable() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }
};


#endif //ANDROIDGLINVESTIGATIONS_MODEL_H