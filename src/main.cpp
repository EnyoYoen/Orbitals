#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "OpenGL.hpp"
#include "ShaderGen.hpp"

namespace {

struct CameraControls {
    float yaw = glm::half_pi<float>();
    float pitch = glm::half_pi<float>();
    float radius = 75.0f;
    bool rotating = false;
    double lastCursorX = 0.0;
    double lastCursorY = 0.0;
};

void scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* controls = static_cast<CameraControls*>(glfwGetWindowUserPointer(window));
    if (!controls) {
        return;
    }

    controls->radius = std::clamp(controls->radius - static_cast<float>(yoffset) * 0.8f, 2.0f, 100.0f);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* controls = static_cast<CameraControls*>(glfwGetWindowUserPointer(window));
    if (!controls || button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }

    if (action == GLFW_PRESS) {
        glfwGetCursorPos(window, &controls->lastCursorX, &controls->lastCursorY);
        controls->rotating = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else if (action == GLFW_RELEASE) {
        controls->rotating = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* controls = static_cast<CameraControls*>(glfwGetWindowUserPointer(window));
    if (!controls || !controls->rotating) {
        return;
    }

    const double dx = controls->lastCursorX - xpos;
    const double dy = controls->lastCursorY - ypos;
    controls->lastCursorX = xpos;
    controls->lastCursorY = ypos;

    const float sensitivity = 0.005f;
    controls->yaw += static_cast<float>(dx) * sensitivity;
    controls->pitch -= static_cast<float>(dy) * sensitivity;
}

} // namespace

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(
        orbitals::ogl::kWindowWidth,
        orbitals::ogl::kWindowHeight,
        "Orbitals",
        nullptr,
        nullptr
    );
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, orbitals::ogl::framebufferSizeCallback);
    CameraControls cameraControls{};
    glfwSetWindowUserPointer(window, &cameraControls);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // GLEW initialization can trigger a benign GL error; clear it once.
    glGetError();

    if (!GLEW_VERSION_3_3) {
        std::cerr << "OpenGL 3.3 is not supported\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glViewport(0, 0, orbitals::ogl::kWindowWidth, orbitals::ogl::kWindowHeight);

    const std::array<float, 18> fullscreenTriangle {
        -1.0f, -1.0f, 0.0f,
         3.0f, -1.0f, 0.0f,
        -1.0f,  3.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
         3.0f, -1.0f, 0.0f,
        -1.0f,  3.0f, 0.0f
    };

    GLuint vao = 0;
    GLuint vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(fullscreenTriangle.size() * sizeof(float)),
        fullscreenTriangle.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    const std::filesystem::path shaderDir = std::filesystem::current_path() / "shaders";
    const std::string& fragmentSource = orbitals::gen::generateShader(shaderDir / "orbitals_template.frag", 2, 1, 1);

#ifdef DEBUG
    {
        std::ofstream generated(shaderDir / "orbitals_out.frag", std::ios::out | std::ios::trunc);
        if (!generated) {
            std::cerr << "Cannot create " << (shaderDir / "orbitals.frag").string() << "\n";
            glfwDestroyWindow(window);
            glfwTerminate();
            return EXIT_FAILURE;
        }
        generated << fragmentSource;
    }
#endif

    const GLuint shaderProgram = orbitals::ogl::createShaderProgram(
        shaderDir / "orbitals.vert",
        fragmentSource
    );

    if (!shaderProgram) {
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    const GLint resolutionLocation = glGetUniformLocation(shaderProgram, "uResolution");
    const GLint yawLocation = glGetUniformLocation(shaderProgram, "uYaw");
    const GLint pitchLocation = glGetUniformLocation(shaderProgram, "uPitch");
    const GLint radiusLocation = glGetUniformLocation(shaderProgram, "uRadius");
    const GLint fovLocation = glGetUniformLocation(shaderProgram, "uFovY");

    float lastTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window)) {
        const float currentTime = static_cast<float>(glfwGetTime());
        const float dt = std::max(currentTime - lastTime, 0.0f);
        lastTime = currentTime;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        const float rotationSpeed = glm::radians(90.0f);
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            cameraControls.yaw -= rotationSpeed * dt;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            cameraControls.yaw += rotationSpeed * dt;
        }
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            cameraControls.pitch += rotationSpeed * dt;
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            cameraControls.pitch -= rotationSpeed * dt;
        }

        const float maxPitch = glm::half_pi<float>() - 0.12f;
        cameraControls.pitch = std::clamp(cameraControls.pitch, -maxPitch, maxPitch);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        if (resolutionLocation >= 0) {
            glUniform2f(
                resolutionLocation,
                static_cast<float>(orbitals::ogl::kWindowWidth),
                static_cast<float>(orbitals::ogl::kWindowHeight)
            );
        }
        if (yawLocation >= 0) {
            glUniform1f(yawLocation, cameraControls.yaw);
        }
        if (pitchLocation >= 0) {
            glUniform1f(pitchLocation, cameraControls.pitch);
        }
        if (radiusLocation >= 0) {
            glUniform1f(radiusLocation, cameraControls.radius);
        }
        if (fovLocation >= 0) {
            glUniform1f(fovLocation, glm::degrees(glm::quarter_pi<float>()));
        }
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(shaderProgram);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
