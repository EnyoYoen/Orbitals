#pragma once

#include <filesystem>
#include <string>
#include <sstream>
#include <stdexcept>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/integer.hpp>
#include <glm/gtc/constants.hpp>

#include "OpenGL.hpp"

namespace orbitals::gen {

double factorial(size_t n) {
    return glm::factorial<size_t>(n);
}

double normalizationConstant(int n, int l, int m) {
    double c = glm::sqrt((8.0f * factorial(n - l - 1)) / (factorial(n + l) * 2 * n * n * n * n));
    c *= glm::pow(2 / n, l);
    c *= glm::sqrt((2*l + 1) * factorial(l - m) / (4 * glm::quarter_pi<double>() * factorial(l + m)));
    return c;
}

void laguerre(std::ostringstream& shader, int alpha, int n) {
    shader << "    float laguerre = 0;\n";
    shader << "    float x = 1.0f;\n";
    for (int k = 0 ; k <= n ; k++) {
        shader << "    laguerre += x * (" << (
            (k % 2 == 0 ? 1 : -1) * factorial(n + alpha + 1) / (factorial(alpha + k + 1) * factorial(n - k) * factorial(k))
        ) << ");\n";
        shader << "    x *= r;\n";
    }
}

void legendre(std::ostringstream& shader, int m, int l) {
    shader << "    float legendre = 0;\n";
    shader << "    x = " << (2 * floor((l-m)/2) == l - m ? "1.0f" : "cosTheta") << ";\n";
    for (int k = static_cast<int>(floor((l - m) / 2.0)) ; k >= 0 ; --k) {
        shader << "    legendre += x * (" << (
            (k % 2 == 0 ? 1 : -1) * factorial(2 * l - 2 * k) / (factorial(l - m - 2 * k) * factorial(l - k) * factorial(k) * glm::pow<double, double>(2, l))
        ) << ");\n";
        shader << "    x *= r;\n";
    }
    shader << "    legendre *= pow(1.0f - cosTheta * cosTheta, " << (m / 2) << ");\n";
}

std::string generatePsi(int n, int l, int m) {
    return R"(float psi(vec3 p)
{
    float r = length(p);
    return 0.5 * p.z * exp(-0.5 * r);
}
)";
    
    if (!(0 <= l && l < n && -l <= m && m <= l)) {
        throw std::runtime_error("Invalid quantum numbers.");
    }

    std::ostringstream shader;
    shader << (R"(
float psi(vec3 p)
{
    float r = length(p);
    if (r < 1e-5) {
        return 0.0;
    }
    float cosTheta = p.z / r;
    float theta = acos(p.z / r);
    float phi = atan(p.y, p.x);

    )");

    shader << "    const int n = " << n << ";\n";
    shader << "    const int l = " << l << ";\n";
    shader << "    const int m = " << m << ";\n\n";

    shader << "    float et = exp(-r / n);\n";
    shader << "    float fa = pow(r, l);\n";
    shader << "    float c = " << normalizationConstant(n, l, m) << ";\n";

    laguerre(shader, 2 * l + 1, n - l - 1);
    legendre(shader, m, l);

    shader << "    return c * et * fa * laguerre * legendre;\n}\n";

    return shader.str();
}

std::string generateShader(std::filesystem::path templateFile, int n, int l, int m) {
    std::string shader = ogl::loadTextFile(templateFile);
    std::string psi = generatePsi(n, l, m);
    
    size_t pos = shader.find("float psi(vec3 p) { return 0.0; }");
    if (pos != std::string::npos) {
        shader.replace(pos, std::string("float psi(vec3 p) { return 0.0; }").length(), psi);
    }
    
    return shader;
}

} // namespace orbitals::gen