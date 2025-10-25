#include "Shader.h"

#include "AndroidOut.h"
#include "Model.h"
#include "Utility.h"

Shader *Shader::loadShader(
        const std::string &vertexSource,
        const std::string &fragmentSource) {
    Shader *shader = nullptr;

    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexSource);
    if (!vertexShader) {
        return nullptr;
    }

    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return nullptr;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);

        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint logLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

            // If we fail to link the shader program, log the result for debugging
            if (logLength) {
                GLchar *log = new GLchar[logLength];
                glGetProgramInfoLog(program, logLength, nullptr, log);
                aout << "Failed to link program with:\n" << log << std::endl;
                delete[] log;
            }

            glDeleteProgram(program);
        } else {
            shader = new Shader(program);
        }
    }

    // The shaders are no longer needed once the program is linked. Release their memory.
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shader;
}

GLuint Shader::loadShader(GLenum shaderType, const std::string &shaderSource) {
    Utility::assertGlError();
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        auto *shaderRawString = (GLchar *) shaderSource.c_str();
        GLint shaderLength = shaderSource.length();
        glShaderSource(shader, 1, &shaderRawString, &shaderLength);
        glCompileShader(shader);

        GLint shaderCompiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &shaderCompiled);

        // If the shader doesn't compile, log the result to the terminal for debugging
        if (!shaderCompiled) {
            GLint infoLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLength);

            if (infoLength) {
                auto *infoLog = new GLchar[infoLength];
                glGetShaderInfoLog(shader, infoLength, nullptr, infoLog);
                aout << "Failed to compile with:\n" << infoLog << std::endl;
                delete[] infoLog;
            }

            glDeleteShader(shader);
            shader = 0;
        }
    }
    return shader;
}

void Shader::activate() const {
    glUseProgram(program_);
}

void Shader::deactivate() const {
    glUseProgram(0);
}


void Shader::setUniformMatrix4(GLint location, const float *matrix) const {
    glUniformMatrix4fv(location, 1, false, matrix);
}

GLint Shader::getUniformLocation(const std::string &uniform) const {
    return glGetUniformLocation(program_, uniform.c_str());
}

void Shader::setUniformb(GLint location, bool value) const {
    glUniform1i(location, value);
}

void Shader::setUniformf(GLint location, float value) const {
    glUniform1f(location, value);
}

void Shader::setUniformi(GLint location, int value) const {
    glUniform1i(location, value);
}

void Shader::setUniform2f(GLint location, const float *value) const {
    glUniform2fv(location, 1, value);
}

void Shader::setUniform2i(GLint location, const int *value) const {
    glUniform2iv(location, 1, value);
}

void Shader::setUniform3f(GLint location, const float *value) const {
    glUniform3fv(location, 1, value);
}

void Shader::setUniform3i(GLint location, const int *value) const {
    glUniform3iv(location, 1, value);
}

void Shader::setUniform4f(GLint location, const float *value) const {
    glUniform4fv(location, 1, value);
}

void Shader::setUniform4i(GLint location, const int *value) const {
    glUniform4iv(location, 1, value);
}
