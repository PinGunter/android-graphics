#ifndef ANDROIDGLINVESTIGATIONS_RENDERER_H
#define ANDROIDGLINVESTIGATIONS_RENDERER_H

#include <EGL/egl.h>
#include <memory>

#include "Model.h"
#include "Shader.h"
#include "glm/glm.hpp"

struct android_app;

class Renderer {
public:
    /*!
     * @param pApp the android_app this Renderer belongs to, needed to configure GL
     */
    inline Renderer(android_app *pApp) :
            app_(pApp),
            display_(EGL_NO_DISPLAY),
            surface_(EGL_NO_SURFACE),
            context_(EGL_NO_CONTEXT),
            width_(0),
            height_(0) {
        initRenderer();
    }

    virtual ~Renderer();

    /*!
     * Handles input from the android_app.
     *
     * Note: this will clear the input queue
     */
    void handleInput();

    /*!
     * Renders all the models in the renderer
     */
    void render();

private:
    /*!
     * Performs necessary OpenGL initialization. Customize this if you want to change your EGL
     * context or application-wide settings.
     */
    void initRenderer();

    /*!
     * @brief we have to check every frame to see if the framebuffer has changed in size. If it has,
     * update the viewport accordingly
     */
    void updateRenderArea();

    /*!
     * Creates the models for this sample. You'd likely load a scene configuration from a file or
     * use some other setup logic in your full game.
     */
    void createModels(int rx, int ry, float r);

    void orbitCamera(float dx, float dy);

    android_app *app_;
    EGLDisplay display_;
    EGLSurface surface_;
    EGLContext context_;
    EGLint width_;
    EGLint height_;

    glm::mat4 projectionMatrix_{1.0f};

    // camera params
    glm::vec3 eye_{0.f, 1.f, 3.f};
    glm::vec3 center_{0.f};
    glm::vec3 up_{0.f, 1.f, 0.f};


    // locations
    GLint viewProjMatLocation_;
    GLint modelMatLocation_;
    GLint colorLocation_;

    std::unique_ptr<Shader> shader_;
    std::vector<Model> models_;

    //input and delta
    glm::vec2 lastPos_{0.f, 0.f};
    glm::vec2 delta_{0.f, 0.f};
};

#endif //ANDROIDGLINVESTIGATIONS_RENDERER_H