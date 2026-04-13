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
    return (double)glm::factorial<size_t>(n);
}

double normalizationConstant(int n, int l, int m) {
    double c = glm::sqrt(pow(2.0/n, 3) * factorial(n-l-1) / (2.0 * n * factorial(n+l)));
    c *= glm::pow(2.0 / n, l);
    c *= glm::sqrt((2*l + 1) * factorial(l - m) / (4 * glm::pi<double>() * factorial(l + m)));
    return c;
}

void laguerre(std::ostringstream& shader, int alpha, int n, int N) {
    shader << "    float laguerre = 0;\n";
    shader << "    float x = 1.0f;\n";
    shader << "    float f = " << (2.0 / (double)N) << ";\n";
    for (int k = 0 ; k <= n ; k++) {
        double coeff = (k % 2 == 0 ? 1 : -1) * factorial(n + alpha + 1) / (factorial(alpha + k + 1) * factorial(n - k) * factorial(k));
        if (coeff == 0) {
            continue;
        } else if (coeff == 1) {
            shader << "    laguerre += x;\n";
        } else if (coeff == -1) {
            shader << "    laguerre -= x;\n";
        } else {
            shader << "    laguerre += x * (" << coeff << ");\n";
        }
        
        if (k < n) {
            shader << "    x *= r * f;\n";
        }
    }
}

void legendre(std::ostringstream& shader, int m, int l) {
    shader << "    float legendre = 0;\n";
    shader << "    x = " << (2 * floor((l-m)/2) == l - m ? "1.0f" : "cosTheta") << ";\n";
    for (int k = static_cast<int>(floor((l - m) / 2.0)) ; k >= 0 ; --k) {
        double coeff = (k % 2 == 0 ? 1 : -1) * factorial(2 * l - 2 * k) / (factorial(l - m - 2 * k) * factorial(l - k) * factorial(k) * glm::pow<double, double>(2, l));
        if (coeff == 0) {
            continue;
        } else if (coeff == 1) {
            shader << "    legendre += x;\n";
        } else if (coeff == -1) {
            shader << "    legendre -= x;\n";
        } else {
            shader << "    legendre += x * (" << coeff << ");\n";
        }
        
        if (k > 0) {
            shader << "    x *= cosTheta * cosTheta;\n";
        }
    }
    if (m > 0) {
        if (m / 2 == 1) {
            shader << "    legendre *= (1.0f - cosTheta * cosTheta);\n";
        } else if (m / 2 > 1) {
            shader << "    legendre *= pow(1.0f - cosTheta * cosTheta, " << (m / 2) << ");\n";
        }
        
        if (m % 2 == 1) {
            shader << "    legendre *= sinTheta;\n";
        }
    }
}

std::string generatePsi(int n, int l, int m, int num = 0) {
    if (!(0 <= l && l < n && -l <= m && m <= l)) {
        throw std::runtime_error("Invalid quantum numbers.");
    }

    std::ostringstream shader;
    shader << "float psi" << (num > 0 ? std::to_string(num) : "") << "(vec3 p)";
    shader << (R"(
{
    float r = length(p);
    if (r < 1e-5) {
        return 0.0;
    }
    float cosTheta = p.z / r;
    float sinTheta = p.x / r;
    float theta = acos(p.z / r);
    float phi = atan(p.y, p.x);

)");

    shader << "    const int n = " << n << ";\n";
    shader << "    const int l = " << l << ";\n";
    shader << "    const int m = " << m << ";\n\n";

    shader << "    float et = exp(-r / n);\n";
    if (l == 0) {
        shader << "    float fa = 1.0f;\n";
    } else if (l == 1) {
        shader << "    float fa = r;\n";
    } else {
        shader << "    float fa = pow(r, l);\n";
    }
    shader << "    float c = " << normalizationConstant(n, l, m) << ";\n";

    laguerre(shader, 2 * l + 1, n - l - 1, n);
    legendre(shader, m, l);

    shader << "    return c * et * fa * laguerre * legendre;\n}";

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

std::string generateMolecularShader(std::filesystem::path templateFile, int n1, int l1, int m1, int n2, int l2, int m2, double distance) {
    std::string shader = ogl::loadTextFile(templateFile);
    std::string psi1 = generatePsi(n1, l1, m1, 1);
    std::string psi2 = generatePsi(n2, l2, m2, 2);

    std::string combinedPsi = psi1 + "\n\n" + psi2 + "\n\n" +
        "float psi(vec3 p)\n"
        "{\n"
        "    vec3 p1 = p + vec3(" + std::to_string(distance / 2.0) + ", 0.0, 0.0);\n"
        "    vec3 p2 = p - vec3(" + std::to_string(distance / 2.0) + ", 0.0, 0.0);\n"
        "    float combined = psi1(p1) + psi2(p2);\n"
        "    return combined;\n"
        "}";
    
    size_t pos = shader.find("float psi(vec3 p) { return 0.0; }");
    if (pos != std::string::npos) {
        shader.replace(pos, std::string("float psi(vec3 p) { return 0.0; }").length(), combinedPsi);
    }
    
    return shader;
}

std::string generateTimeShader(std::filesystem::path templateFile, int n1, int l1, int m1, int n2, int l2, int m2) {
    std::string shader = ogl::loadTextFile(templateFile);
    std::string psi1 = generatePsi(n1, l1, m1, 1);
    std::string psi2 = generatePsi(n2, l2, m2, 2);

    std::string combinedPsi = psi1 + "\n\n" + psi2 + "\n\n" +
        "vec2 psi(vec3 p)\n"
        "{\n"
        "    float t = uTime * 5.0;\n"
        "    float E1 = energy(" + std::to_string(n1) + ");\n"
        "    float E2 = energy(" + std::to_string(n2) + ");\n"
        "    vec2 a = vec2(psi1(p), 0.0);\n"
        "    vec2 b = vec2(psi2(p), 0.0);\n"
        "    a = complex_mul(a, phase(E1, t));\n"
        "    b = complex_mul(b, phase(E2, t));\n"
        "    return a + b;\n"
        "}";
    
    size_t pos = shader.find("vec2 psi(vec3 p) { return vec2(0.0); }");
    if (pos != std::string::npos) {
        shader.replace(pos, std::string("vec2 psi(vec3 p) { return vec2(0.0); }").length(), combinedPsi);
    }
    
    return shader;
}

} // namespace orbitals::gen