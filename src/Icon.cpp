#include "Icon.hpp"

#include "OpenGL.hpp"

#include <filesystem>
#include <iostream>
#include <system_error>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace orbitals::icon {

bool loadPngRgba(const std::filesystem::path& path, int& width, int& height, std::vector<unsigned char>& rgba) {
    int channels = 0;
    stbi_uc* raw = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!raw) {
        return false;
    }

    const std::size_t bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    rgba.assign(raw, raw + bytes);
    stbi_image_free(raw);
    return true;
}

void setWindowIconFromPng(GLFWwindow* window, int argc, char** argv) {
    std::filesystem::path executableDir;
    if (argc > 0 && argv && argv[0] && argv[0][0] != '\0') {
        std::error_code ec;
        const std::filesystem::path executablePath = std::filesystem::absolute(std::filesystem::path(argv[0]), ec);
        if (!ec) {
            executableDir = executablePath.parent_path();
        }
    }

    int iconWidth = 0;
    int iconHeight = 0;
    std::vector<unsigned char> rgba;

    const std::vector<std::filesystem::path> candidates {
        std::filesystem::current_path() / "images" / "icon.png",
        std::filesystem::current_path() / "icon.png",
        executableDir / "images" / "icon.png",
        executableDir / "icon.png",
        executableDir.parent_path() / "images" / "icon.png"
    };

    for (const auto& path : candidates) {
        if (!loadPngRgba(path, iconWidth, iconHeight, rgba)) {
            continue;
        }

        GLFWimage image{};
        image.width = iconWidth;
        image.height = iconHeight;
        image.pixels = rgba.data();
        glfwSetWindowIcon(window, 1, &image);
        return;
    }

    std::cerr << "Window icon not found or PNG load failed.\n";
}

} // namespace orbitals::icon
