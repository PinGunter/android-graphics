#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>
#include <android/imagedecoder.h>

#include "AndroidOut.h"
#include "Shader.h"
#include "TextureAsset.h"
#include "glm/gtc/type_ptr.hpp"

//! executes glGetString and outputs the result to logcat
#define PRINT_GL_STRING(s) {aout << #s": "<< glGetString(s) << std::endl;}

/*!
 * @brief if glGetString returns a space separated list of elements, prints each one on a new line
 *
 * This works by creating an istringstream of the input c-style string. Then that is used to create
 * a vector -- each element of the vector is a new element in the input string. Finally a foreach
 * loop consumes this and outputs it to logcat using @a aout
 */
#define PRINT_GL_STRING_AS_LIST(s) { \
std::istringstream extensionStream((const char *) glGetString(s));\
std::vector<std::string> extensionList(\
        std::istream_iterator<std::string>{extensionStream},\
        std::istream_iterator<std::string>());\
aout << #s":\n";\
for (auto& extension: extensionList) {\
    aout << extension << "\n";\
}\
aout << std::endl;\
}

Renderer::~Renderer() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

void Renderer::render() {
    // Check to see if the surface has changed size. This is _necessary_ to do every frame when
    // using immersive mode as you'll get no other notification that your renderable area has
    // changed.
    updateRenderArea();

    float time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - startT_).count() / 1000.0f;

    shader_->activate();

    shader_->setUniform3f(locStartColor, glm::value_ptr(startColor));
    shader_->setUniform3f(locEndColor, glm::value_ptr(endColor));
    shader_->setUniformf(locProgress, progress);
    shader_->setUniformMatrix4(locMVPMatrix, glm::value_ptr(glm::mat4(1.0f)));
    shader_->setUniform2f(locResolution, glm::value_ptr(glm::vec2(width_, height_)));

    // clear the color buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render all the models. There's no depth testing in this sample so they're accepted in the
    // order provided. But the sample EGL setup requests a 24 bit depth buffer so you could
    // configure it at the end of initRenderer
    if (!models_.empty()) {
        for (const auto &model: models_) {
            model.draw();
        }
    }

    shader_->deactivate();

    // Present the rendered image. This is an implicit glFlush.
    auto swapResult = eglSwapBuffers(display_, surface_);
    assert(swapResult == EGL_TRUE);
}

void Renderer::initRenderer() {
    // Choose your render attributes
    constexpr EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
    };

    // The default display is probably what you want on Android
    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    // figure out how many configs there are
    EGLint numConfigs;
    eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);

    // get the list of configurations
    std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
    eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs, &numConfigs);

    // Find a config we like.
    // Could likely just grab the first if we don't care about anything else in the config.
    // Otherwise hook in your own heuristic
    auto config = *std::find_if(
            supportedConfigs.get(),
            supportedConfigs.get() + numConfigs,
            [&display](const EGLConfig &config) {
                EGLint red, green, blue, depth;
                if (eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red)
                    && eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green)
                    && eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue)
                    && eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depth)) {

                    aout << "Found config with " << red << ", " << green << ", " << blue << ", "
                         << depth << std::endl;
                    return red == 8 && green == 8 && blue == 8 && depth == 24;
                }
                return false;
            });

    aout << "Found " << numConfigs << " configs" << std::endl;
    aout << "Chose " << config << std::endl;

    // create the proper window surface
    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    EGLSurface surface = eglCreateWindowSurface(display, config, app_->window, nullptr);

    // Create a GLES 3 context
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttribs);

    // get some window metrics
    auto madeCurrent = eglMakeCurrent(display, surface, surface, context);
    assert(madeCurrent);

    display_ = display;
    surface_ = surface;
    context_ = context;

    // make width and height invalid so it gets updated the first frame in @a updateRenderArea()
    width_ = -1;
    height_ = -1;

    PRINT_GL_STRING(GL_VENDOR);
    PRINT_GL_STRING(GL_RENDERER);
    PRINT_GL_STRING(GL_VERSION);
    PRINT_GL_STRING_AS_LIST(GL_EXTENSIONS);

    std::string vertexSource = loadFile("vertex.glsl");
    std::string fragmentSource = loadFile("fragment.glsl");
    shader_ = std::unique_ptr<Shader>(Shader::loadShader(vertexSource, fragmentSource));
    assert(shader_);

    // Note: there's only one shader in this demo, so I'll activate it here. For a more complex game
    // you'll want to track the active shader and activate/deactivate it as necessary
    shader_->activate();

    // setup any other gl related global states
    glClearColor(0.f, 0.f, 0.f, 1.f);

    // enable alpha globally for now, you probably don't want to do this in a game
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // get some demo models into memory
//    createSphere(12, 12, 1);
//    createSphere(3, 4, 0.5);
    createModel();

    // resolution, time, cameraPos, lookAt
    locStartColor = shader_->getUniformLocation("uStartColor");
    locEndColor = shader_->getUniformLocation("uEndColor");
    locProgress = shader_->getUniformLocation("uProgress");
    locMVPMatrix = shader_->getUniformLocation("uModelViewProjectionMatrix");
    locResolution = shader_->getUniformLocation("uResolution");

    startT_ = std::chrono::high_resolution_clock::now();
}

void Renderer::updateRenderArea() {
    EGLint width;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);

    EGLint height;
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);

    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width, height);
    }
}


/**
 * @brief Create any demo models we want for this demo.
 */
void Renderer::createModel() {
    std::vector<Vertex> vertices{
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
            {{1.0f,  -1.0f, 0.0f}, {1.0f, 0.0f}},
            {{1.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
            {{-1.0f, 1.0f,  0.0f}, {0.0f, 1.0f}}
    };
    std::vector<uint16_t> indices{
            0, 1, 2,
            2, 3, 0
    };

    models_.emplace_back(std::make_shared<Model>(vertices, indices));
}

void Renderer::handleInput() {
    // handle all queued inputs
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) {
        // no inputs yet.
        return;
    }

    pointerDelta_ = glm::vec2(0.0f);

    // handle motion events (motionEventsCounts can be 0).
    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto action = motionEvent.action;

        // Find the pointer index, mask and bitshift to turn it into a readable value.
        auto pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        aout << "Pointer(s): ";

        // get the x and y position of this event if it is not ACTION_MOVE.
        auto &pointer = motionEvent.pointers[pointerIndex];
        auto x = GameActivityPointerAxes_getX(&pointer);
        auto y = GameActivityPointerAxes_getY(&pointer);
        pointerDelta_ = {x - lastPos_.x, y - lastPos_.y};
        lastPos_ = {x, y};

        // determine the action type and process the event accordingly.
        switch (action & AMOTION_EVENT_ACTION_MASK) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                aout << "(" << pointer.id << ", " << x << ", " << y << ") "
                     << "Pointer Down";

                break;

            case AMOTION_EVENT_ACTION_CANCEL:
                // treat the CANCEL as an UP event: doing nothing in the app, except
                // removing the pointer from the cache if pointers are locally saved.
                // code pass through on purpose.
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                aout << "(" << pointer.id << ", " << x << ", " << y << ") "
                     << "Pointer Up";
                break;

            case AMOTION_EVENT_ACTION_MOVE:
                // There is no pointer index for ACTION_MOVE, only a snapshot of
                // all active pointers; app needs to cache previous active pointers
                // to figure out which ones are actually moved.
//                for (auto index = 0; index < motionEvent.pointerCount; index++) {
//                    pointer = motionEvent.pointers[index];
//                    x = GameActivityPointerAxes_getX(&pointer);
//                    y = GameActivityPointerAxes_getY(&pointer);
//                    aout << "(" << pointer.id << ", " << x << ", " << y << ")";
//
//                    if (index != (motionEvent.pointerCount - 1)) aout << ",";
//                    aout << " ";
//                    pointerDelta_ = {x - lastPos_.x, y - lastPos_.y};
//                    lastPos_ = {x, y};
//                    orbitCamera(pointerDelta_.x, pointerDelta_.y);
//                }

                pointer = motionEvent.pointers[0];
                x = GameActivityPointerAxes_getX(&pointer);
                y = GameActivityPointerAxes_getY(&pointer);
                pointer_ = {x, y};
                progress = x / width_;
                aout << "(" << pointer.id << ", " << x << ", " << y << ")";
                aout << " ";

                orbitCamera(pointerDelta_.x, pointerDelta_.y);

                aout << "Pointer Move";
                break;
            default:
                aout << "Unknown MotionEvent Action: " << action;
        }
        aout << std::endl;
    }
    // clear the motion input count in this buffer for main thread to re-use.
    android_app_clear_motion_events(inputBuffer);

    // handle input key events.
    for (auto i = 0; i < inputBuffer->keyEventsCount; i++) {
        auto &keyEvent = inputBuffer->keyEvents[i];
        aout << "Key: " << keyEvent.keyCode << " ";
        switch (keyEvent.action) {
            case AKEY_EVENT_ACTION_DOWN:
                aout << "Key Down";
                break;
            case AKEY_EVENT_ACTION_UP:
                aout << "Key Up";
                break;
            case AKEY_EVENT_ACTION_MULTIPLE:
                // Deprecated since Android API level 29.
                aout << "Multiple Key Actions";
                break;
            default:
                aout << "Unknown KeyEvent Action: " << keyEvent.action;
        }
        aout << std::endl;
    }
    // clear the key input count too.
    android_app_clear_key_events(inputBuffer);
}

void Renderer::orbitCamera(float dx, float dy) {
    if (dx == 0 && dy == 0)
        return;

    dx *= 0.003f;
    dy *= 0.003f;

    // Get the length of sight
    glm::vec3 centerToEye(eye_ - center_);
    float radius = glm::length(centerToEye);
    centerToEye = glm::normalize(centerToEye);
    glm::vec3 direction = centerToEye;

    // Find the rotation around the UP axis (Y)
    glm::mat4 rot_y = glm::rotate(glm::mat4(1), -dx, up_);

    // Apply the (Y) rotation to the eye-center vector
    centerToEye = rot_y * glm::vec4(centerToEye, 0);

    // Find the rotation around the X vector: cross between eye-center and up (X)
    glm::vec3 axe_x = glm::normalize(glm::cross(up_, direction));
    glm::mat4 rot_x = glm::rotate(glm::mat4(1), -dy, axe_x);

    // Apply the (X) rotation to the eye-center vector
    glm::vec3 vect_rot = rot_x * glm::vec4(centerToEye, 0);

    if (glm::sign(vect_rot.x) == glm::sign(centerToEye.x))
        centerToEye = vect_rot;

    // Make the vector as long as it was originally
    centerToEye *= radius;

    // Finding the new position
    glm::vec3 newPosition = centerToEye + center_;

    eye_ = newPosition;
}

std::string Renderer::loadFile(const std::string &file) {
    AAsset *asset = AAssetManager_open(app_->activity->assetManager, file.c_str(),
                                       AASSET_MODE_BUFFER);
    assert(asset);

    size_t fileSize = AAsset_getLength(asset);
    char *buffer = new char[fileSize + 1];
    AAsset_read(asset, buffer, fileSize);
    buffer[fileSize] = '\0';

    std::string output(buffer);

    delete[] buffer;
    AAsset_close(asset);

    return output;
}
