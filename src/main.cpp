#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "Icon.hpp"
#include "OpenGL.hpp"
#include "ShaderGen.hpp"

bool gUiCapturesMouse = false;

enum class SimulationMode {
    Normal = 0,
    Molecular = 1,
    Timed = 2
};

struct CameraControls {
    float yaw = glm::half_pi<float>();
    float pitch = glm::half_pi<float>();
    float radius = 200.0f;
    bool rotating = false;
    double lastCursorX = 0.0;
    double lastCursorY = 0.0;
};

void scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* controls = static_cast<CameraControls*>(glfwGetWindowUserPointer(window));
    if (!controls) {
        return;
    }

    controls->radius = std::clamp(controls->radius - static_cast<float>(yoffset) * 4.0f, 2.0f, 300.0f);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* controls = static_cast<CameraControls*>(glfwGetWindowUserPointer(window));
    if (!controls || button != GLFW_MOUSE_BUTTON_LEFT || gUiCapturesMouse) {
        if (controls && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
            controls->rotating = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
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

int main(int argc, char** argv) {
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

    orbitals::icon::setWindowIconFromPng(window, argc, argv);

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
    int quantumN = 4;
    int quantumL = 2;
    int quantumM = 1;
    std::string lastShaderError;

    SimulationMode simulationMode = SimulationMode::Normal;
    int quantumN2 = 5;
    int quantumL2 = 2;
    int quantumM2 = 1;
    double molecularDistance = 10.0;

    auto generateFragmentSource = [&]() {
        if (simulationMode == SimulationMode::Molecular) {
            return orbitals::gen::generateMolecularShader(
                shaderDir / "orbitals_template.frag",
                quantumN,
                quantumL,
                quantumM,
                quantumN2,
                quantumL2,
                quantumM2,
                molecularDistance
            );
        }
        if (simulationMode == SimulationMode::Timed) {
            return orbitals::gen::generateTimeShader(
                shaderDir / "orbitals_time_template.frag",
                quantumN,
                quantumL,
                quantumM,
                quantumN2,
                quantumL2,
                quantumM2
            );
        }

        return orbitals::gen::generateShader(
            shaderDir / "orbitals_template.frag",
            quantumN,
            quantumL,
            quantumM
        );
    };

    std::string fragmentSource = generateFragmentSource();

#ifndef DEBUG
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

    GLuint shaderProgram = orbitals::ogl::createShaderProgram(
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

    GLint resolutionLocation = glGetUniformLocation(shaderProgram, "uResolution");
    GLint yawLocation = glGetUniformLocation(shaderProgram, "uYaw");
    GLint pitchLocation = glGetUniformLocation(shaderProgram, "uPitch");
    GLint radiusLocation = glGetUniformLocation(shaderProgram, "uRadius");
    GLint fovLocation = glGetUniformLocation(shaderProgram, "uFovY");
    GLint timeLocation = glGetUniformLocation(shaderProgram, "uTime");

    auto refreshUniformLocations = [&]() {
        resolutionLocation = glGetUniformLocation(shaderProgram, "uResolution");
        yawLocation = glGetUniformLocation(shaderProgram, "uYaw");
        pitchLocation = glGetUniformLocation(shaderProgram, "uPitch");
        radiusLocation = glGetUniformLocation(shaderProgram, "uRadius");
        fovLocation = glGetUniformLocation(shaderProgram, "uFovY");
        timeLocation = glGetUniformLocation(shaderProgram, "uTime");
    };

    auto rebuildShader = [&]() {
        try {
            std::string nextSource = generateFragmentSource();

#ifndef DEBUG
            {
                std::ofstream generated(shaderDir / "orbitals_out.frag", std::ios::out | std::ios::trunc);
                if (generated) {
                    generated << nextSource;
                }
            }
#endif

            const GLuint nextProgram = orbitals::ogl::createShaderProgram(shaderDir / "orbitals.vert", nextSource);
            if (nextProgram != 0) {
                glDeleteProgram(shaderProgram);
                shaderProgram = nextProgram;
                refreshUniformLocations();
                lastShaderError.clear();
            } else {
                lastShaderError = "Shader compilation failed. See terminal output.";
            }
        } catch (const std::exception& ex) {
            lastShaderError = ex.what();
        }
    };

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    float lastTime = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window)) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const float currentTime = static_cast<float>(glfwGetTime());
        const float dt = std::max(currentTime - lastTime, 0.0f);
        lastTime = currentTime;

        gUiCapturesMouse = ImGui::GetIO().WantCaptureMouse;

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

        bool dirtyQuantum = false;
        ImGui::Begin("Quantum Numbers");

        static int selectedSimulation = static_cast<int>(simulationMode);
        if (ImGui::Combo("Simulation", &selectedSimulation, "Normal\0Molecular\0Timed\0")) {
            simulationMode = static_cast<SimulationMode>(selectedSimulation);
            dirtyQuantum = true;
        }

        ImGui::Text("Orbital 1:");
        dirtyQuantum |= ImGui::SliderInt("n##1", &quantumN, 1, 8);

        quantumL = std::clamp(quantumL, 0, quantumN - 1);
        dirtyQuantum |= ImGui::SliderInt("l##1", &quantumL, 0, quantumN - 1);

        quantumM = std::clamp(quantumM, -quantumL, quantumL);
        dirtyQuantum |= ImGui::SliderInt("m##1", &quantumM, -quantumL, quantumL);

        ImGui::Text("Current: n=%d, l=%d, m=%d", quantumN, quantumL, quantumM);

        if (simulationMode != SimulationMode::Normal) {
            ImGui::Separator();
            ImGui::Text("Orbital 2:");
            dirtyQuantum |= ImGui::SliderInt("n##2", &quantumN2, 1, 8);

            quantumL2 = std::clamp(quantumL2, 0, quantumN2 - 1);
            dirtyQuantum |= ImGui::SliderInt("l##2", &quantumL2, 0, quantumN2 - 1);

            quantumM2 = std::clamp(quantumM2, -quantumL2, quantumL2);
            dirtyQuantum |= ImGui::SliderInt("m##2", &quantumM2, -quantumL2, quantumL2);

            ImGui::Text("Current: n=%d, l=%d, m=%d", quantumN2, quantumL2, quantumM2);

            if (simulationMode == SimulationMode::Molecular) {
                double distMin = 10.0;
                double distMax = 75.0;
                dirtyQuantum |= ImGui::SliderScalar("Distance##mol", ImGuiDataType_Double, &molecularDistance, &distMin, &distMax, "%.2f");
            }
        }

        if (!lastShaderError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", lastShaderError.c_str());
        }
        ImGui::End();

        if (dirtyQuantum) {
            rebuildShader();
        }

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
        if (timeLocation >= 0) {
            glUniform1f(timeLocation, currentTime);
        }
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteProgram(shaderProgram);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
