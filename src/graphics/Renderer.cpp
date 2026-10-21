#include "Renderer.hpp"
#include "ArtTexture.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gfx {
namespace {

constexpr float pi = 3.14159265358979323846f;

std::string readText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Cannot open shader: " + path.string());
    }
    std::ostringstream text;
    text << stream.rdbuf();
    return text.str();
}

GLuint compileShader(GLenum kind, const std::string& source,
                     const std::filesystem::path& path) {
    const GLuint shader = glCreateShader(kind);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Shader compile error in " + path.string() + ":\n" + log);
    }
    return shader;
}

struct Program {
    GLuint id = 0;
    ~Program() { if (id) glDeleteProgram(id); }

    void load(const std::filesystem::path& vertexPath,
              const std::filesystem::path& fragmentPath) {
        const std::string vertexSource = readText(vertexPath);
        const std::string fragmentSource = readText(fragmentPath);
        const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource, vertexPath);
        GLuint fragment = 0;
        try {
            fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);
        } catch (...) {
            glDeleteShader(vertex);
            throw;
        }
        id = glCreateProgram();
        glAttachShader(id, vertex);
        glAttachShader(id, fragment);
        glLinkProgram(id);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        GLint success = GL_FALSE;
        glGetProgramiv(id, GL_LINK_STATUS, &success);
        if (success != GL_TRUE) {
            GLint length = 0;
            glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
            std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
            glGetProgramInfoLog(id, length, nullptr, log.data());
            throw std::runtime_error("Table shader link error:\n" + log);
        }
    }
};

struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;

    Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept { *this = std::move(other); }
    Mesh& operator=(Mesh&& other) noexcept {
        if (this != &other) {
            release();
            vao = std::exchange(other.vao, 0);
            vbo = std::exchange(other.vbo, 0);
            ebo = std::exchange(other.ebo, 0);
            indexCount = std::exchange(other.indexCount, 0);
        }
        return *this;
    }
    ~Mesh() { release(); }
    void release() {
        if (ebo) glDeleteBuffers(1, &ebo);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = vbo = ebo = 0;
    }
    void draw() const {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    }
    void update(const std::vector<float>& vertices) const {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data());
    }
};

Mesh makeMesh(const std::vector<float>& vertices,
              const std::vector<std::uint32_t>& indices) {
    Mesh mesh;
    mesh.indexCount = static_cast<GLsizei>(indices.size());
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                 indices.data(), GL_STATIC_DRAW);
    constexpr GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(6 * sizeof(float)));
    glBindVertexArray(0);
    return mesh;
}

Mesh makeCube() {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    const auto face = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                          glm::vec3 normal) {
        const auto offset = static_cast<std::uint32_t>(vertices.size() / 8);
        const std::array<glm::vec3, 4> corners{a, b, c, d};
        const std::array<glm::vec2, 4> uvs{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
        for (std::size_t i = 0; i < corners.size(); ++i) {
            const auto p = corners[i];
            vertices.insert(vertices.end(), {p.x, p.y, p.z, normal.x, normal.y,
                                             normal.z, uvs[i].x, uvs[i].y});
        }
        indices.insert(indices.end(), {offset, offset + 1, offset + 2,
                                       offset, offset + 2, offset + 3});
    };
    face({-.5f, .5f, .5f}, {.5f, .5f, .5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f}, {0, 1, 0});
    face({-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, -.5f, .5f}, {-.5f, -.5f, .5f}, {0, -1, 0});
    face({-.5f, -.5f, .5f}, {.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}, {0, 0, 1});
    face({.5f, -.5f, -.5f}, {-.5f, -.5f, -.5f}, {-.5f, .5f, -.5f}, {.5f, .5f, -.5f}, {0, 0, -1});
    face({-.5f, -.5f, -.5f}, {-.5f, -.5f, .5f}, {-.5f, .5f, .5f}, {-.5f, .5f, -.5f}, {-1, 0, 0});
    face({.5f, -.5f, .5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {.5f, .5f, .5f}, {1, 0, 0});
    return makeMesh(vertices, indices);
}

Mesh makeTopPlane() {
    return makeMesh({
        -.5f, .52f, .5f, 0, 1, 0, 0, 0,
         .5f, .52f, .5f, 0, 1, 0, 1, 0,
         .5f, .52f,-.5f, 0, 1, 0, 1, 1,
        -.5f, .52f,-.5f, 0, 1, 0, 0, 1,
    }, {0, 1, 2, 0, 2, 3});
}

Mesh makeCylinder(int segments) {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    const auto vertex = [&](float x, float y, float z, float nx, float ny, float nz,
                            float u, float v) {
        vertices.insert(vertices.end(), {x, y, z, nx, ny, nz, u, v});
    };
    for (int i = 0; i <= segments; ++i) {
        const float fraction = static_cast<float>(i) / static_cast<float>(segments);
        const float angle = fraction * 2.0f * pi;
        const float x = std::cos(angle);
        const float z = std::sin(angle);
        vertex(x, -.5f, z, x, 0, z, fraction, 0);
        vertex(x, .5f, z, x, 0, z, fraction, 1);
        if (i < segments) {
            const auto n = static_cast<std::uint32_t>(2 * i);
            indices.insert(indices.end(), {n, n + 1, n + 3, n, n + 3, n + 2});
        }
    }
    for (int side = 0; side < 2; ++side) {
        const float y = side ? .5f : -.5f;
        const float normalY = side ? 1.0f : -1.0f;
        const auto center = static_cast<std::uint32_t>(vertices.size() / 8);
        vertex(0, y, 0, 0, normalY, 0, .5f, .5f);
        for (int i = 0; i <= segments; ++i) {
            const float angle = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * pi;
            const float x = std::cos(angle);
            const float z = std::sin(angle);
            vertex(x, y, z, 0, normalY, 0, x * .5f + .5f, z * .5f + .5f);
            if (i < segments) {
                const auto n = center + 1 + static_cast<std::uint32_t>(i);
                if (side) indices.insert(indices.end(), {center, n + 1, n});
                else indices.insert(indices.end(), {center, n, n + 1});
            }
        }
    }
    return makeMesh(vertices, indices);
}

Mesh makeSphere(int slices, int rings, float start = 0.0f, float span = 2.0f * pi) {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    for (int ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float latitude = v * pi;
        for (int slice = 0; slice <= slices; ++slice) {
            const float u = static_cast<float>(slice) / static_cast<float>(slices);
            const float longitude = start + u * span;
            const glm::vec3 p(std::sin(latitude) * std::cos(longitude),
                              std::cos(latitude), std::sin(latitude) * std::sin(longitude));
            vertices.insert(vertices.end(), {p.x, p.y, p.z, p.x, p.y, p.z, u, v});
            if (ring < rings && slice < slices) {
                const auto n = static_cast<std::uint32_t>(ring * (slices + 1) + slice);
                const auto next = n + static_cast<std::uint32_t>(slices + 1);
                indices.insert(indices.end(), {n, next, n + 1, n + 1, next, next + 1});
            }
        }
    }
    return makeMesh(vertices, indices);
}

Mesh makeHorn() {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    constexpr int rings = 18, sides = 20;
    for (int ring = 0; ring <= rings; ++ring) {
        const float t = static_cast<float>(ring) / rings;
        const glm::vec3 center(.92f * t, .72f * std::sin(t * pi * .88f), .065f * std::sin(t * pi));
        const glm::vec3 tangent = glm::normalize(glm::vec3(.92f, .72f * pi * .88f * std::cos(t * pi * .88f), .065f * pi * std::cos(t * pi)));
        const glm::vec3 across(-tangent.y, tangent.x, 0);
        for (int side = 0; side <= sides; ++side) {
            const float u = static_cast<float>(side) / sides;
            const float angle = u * 2 * pi;
            const float radius = .17f * std::pow(1.0f - t, .80f) + .008f
                + .008f * std::sin(angle * 9 + t * 15) * (1 - t);
            const glm::vec3 normal = across * std::cos(angle) + glm::vec3(0, 0, std::sin(angle));
            const glm::vec3 p = center + normal * radius;
            vertices.insert(vertices.end(), {p.x, p.y, p.z, normal.x, normal.y, normal.z, u, t});
            if (ring < rings && side < sides) {
                const auto k = static_cast<std::uint32_t>(ring * (sides + 1) + side);
                const auto next = k + sides + 1;
                indices.insert(indices.end(), {k, next, k + 1, k + 1, next, next + 1});
            }
        }
    }
    return makeMesh(vertices, indices);
}

Mesh makeRing(int segments, bool arch) {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / segments;
        const float angle = t * (arch ? pi : 2 * pi);
        for (const float radius : {.88f, 1.0f}) {
            const float x = std::cos(angle) * radius, y = std::sin(angle) * radius;
            if (arch) vertices.insert(vertices.end(), {x, y, 0, 0, 0, 1, t, radius});
            else vertices.insert(vertices.end(), {x, 0, y, 0, 1, 0, t, radius});
        }
        if (i < segments) {
            const auto k = static_cast<std::uint32_t>(i * 2);
            indices.insert(indices.end(), {k, k + 1, k + 2, k + 1, k + 3, k + 2});
        }
    }
    return makeMesh(vertices, indices);
}

struct SurfaceData {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
};

template <typename Shape>
SurfaceData surfaceData(int columns, int rows, Shape shape, bool reverse = false) {
    SurfaceData data;
    data.vertices.reserve(static_cast<std::size_t>((columns + 1) * (rows + 1) * 8));
    for (int row = 0; row <= rows; ++row) {
        const float v = static_cast<float>(row) / static_cast<float>(rows);
        for (int column = 0; column <= columns; ++column) {
            const float u = static_cast<float>(column) / static_cast<float>(columns);
            const glm::vec3 p = shape(u, v);
            const glm::vec3 du = shape(u + .0005f, v) - shape(u - .0005f, v);
            const glm::vec3 dv = shape(u, v + .0005f) - shape(u, v - .0005f);
            glm::vec3 normal = glm::cross(du, dv);
            normal = glm::length(normal) > .000000000001f ? glm::normalize(normal) : glm::vec3(0, 0, 1);
            if (reverse) normal = -normal;
            data.vertices.insert(data.vertices.end(), {p.x, p.y, p.z, normal.x, normal.y, normal.z, u, v});
            if (row < rows && column < columns) {
                const auto i = static_cast<std::uint32_t>(row * (columns + 1) + column);
                const auto next = i + static_cast<std::uint32_t>(columns + 1);
                data.indices.insert(data.indices.end(), {i, i + 1, next, i + 1, next + 1, next});
            }
        }
    }
    return data;
}

Mesh surfaceMesh(const SurfaceData& data) { return makeMesh(data.vertices, data.indices); }

void appendSurface(SurfaceData& destination, SurfaceData addition) {
    const auto offset = static_cast<std::uint32_t>(destination.vertices.size() / 8);
    destination.vertices.insert(destination.vertices.end(), addition.vertices.begin(), addition.vertices.end());
    for (const auto index : addition.indices) destination.indices.push_back(offset + index);
}

template <typename Path>
SurfaceData cordSurface(int segments, Path path, float radius) {
    return surfaceData(8, segments, [=](float u, float t) {
        const glm::vec3 center = path(t);
        const glm::vec3 tangent = glm::normalize(path(t + .0005f) - path(t - .0005f));
        glm::vec3 across = glm::cross(tangent, glm::vec3(0, 0, 1));
        if (glm::length(across) < .001f) across = glm::cross(tangent, glm::vec3(0, 1, 0));
        across = glm::normalize(across);
        const glm::vec3 around = glm::cross(tangent, across);
        return center + (across * std::cos(u * 2 * pi) + around * std::sin(u * 2 * pi)) * radius;
    });
}

glm::vec3 porcelainShape(float s, float t) {
    const float u = std::clamp(s * 2 - 1, -1.01f, 1.01f), v = t * 2 - 1;
    // An oval jaw and a short, gently sculpted bridge keep the mask porcelain,
    // rather than turning its lower half into a pointed theatrical skull.
    const float oval = std::sqrt(std::max(0.0f, 1 - v * v));
    const float width = oval * (.86f + .14f * glm::smoothstep(-.85f, -.05f, v));
    const float dome = std::sqrt(std::max(.001f, 1 - u * u));
    const float nose = .077f * std::exp(-u * u * 70) * std::exp(-(v + .025f) * (v + .025f) * 24);
    const float cheek = .030f * std::exp(-std::pow(std::abs(u) - .50f, 2.0f) * 22)
        * std::exp(-std::pow(v + .20f, 2.0f) * 26);
    const float brow = .018f * std::exp(-std::pow(std::abs(u) - .39f, 2.0f) * 32)
        * std::exp(-std::pow(v - .37f, 2.0f) * 70);
    return {u * .415f * width, v * .46f,
            .19f * dome * std::sqrt(std::max(.01f, 1 - v * v * .45f)) + nose + cheek + brow};
}

glm::vec3 leadPorcelainShape(float s, float t) {
    const float u = std::clamp(s * 2 - 1, -1.01f, 1.01f), v = t * 2 - 1;
    const float outline = std::sqrt(std::max(0.0f, 1 - v * v));
    const float jaw = .87f + .13f * glm::smoothstep(-.8f, -.05f, v);
    // A thin, elongated shell: no modeled nose or rounded toy cranium.
    const float curve = std::sqrt(std::max(.001f, 1 - u * u));
    return {u * .192f * outline * jaw, v * .280f,
            .103f * curve * std::sqrt(std::max(.01f, 1 - v * v * .25f))};
}

glm::vec3 jacketShape(float s, float t) {
    const float a = s * 2 * pi;
    const float chest = std::exp(-std::pow((t - .76f) / .23f, 2.0f));
    const float neck = glm::smoothstep(.82f, 1.0f, t);
    const float radius = (.45f + .35f * chest) * (1 - neck * .75f);
    const float folds = (.009f + .025f * (1 - t)) * std::sin(a * 13 + t * 7)
        + .007f * std::sin(a * 23 - t * 16);
    return {std::cos(a) * (radius + folds), -.14f + t * 1.65f,
            std::sin(a) * (.29f + chest * .07f + folds) - .05f * std::sin(t * pi)};
}

SurfaceData sleeveSurface(glm::vec3 shoulder, glm::vec3 elbow, glm::vec3 wrist) {
    return surfaceData(32, 40, [=](float u, float t) {
        const float a = u * 2 * pi;
        const glm::vec3 p = (1 - t) * (1 - t) * shoulder + 2 * (1 - t) * t * elbow + t * t * wrist;
        const glm::vec3 tangent = glm::normalize((elbow - shoulder) * (1 - t) + (wrist - elbow) * t);
        const glm::vec3 across = glm::normalize(glm::cross(tangent, glm::vec3(0, 0, 1)));
        const glm::vec3 around = glm::cross(tangent, across);
        const float folds = .014f * std::sin(t * 34 + std::sin(a * 3) * .6f)
            * (.35f + .65f * std::sin(t * pi));
        const float radius = glm::mix(.225f, .083f, t) + .025f * std::sin(t * pi) + folds;
        return p + (across * std::cos(a) + around * std::sin(a)) * radius;
    });
}

SurfaceData gloveSurface() {
    SurfaceData data = surfaceData(32, 24, [](float u, float t) {
        const float a = u * 2 * pi;
        const float close = 1.0f - .90f * glm::smoothstep(.78f, 1.0f, t);
        const float width = (.065f + .068f * std::sin(t * pi * .85f)) * close;
        return glm::vec3(std::cos(a) * width, std::sin(a) * (.045f + .015f * std::sin(t * pi)) * close, t * .28f);
    });
    for (int finger = 0; finger < 5; ++finger) {
        appendSurface(data, surfaceData(16, 18, [=](float u, float t) {
            const float a = u * 2 * pi;
            const bool thumb = finger == 4;
            const float x = thumb ? -.14f - t * .06f : (static_cast<float>(finger) - 1.5f) * .062f;
            const float length = thumb ? .17f : .24f - std::abs(static_cast<float>(finger) - 1) * .026f;
            const float radius = .028f * std::sqrt(std::max(.001f, 1 - t * t));
            return glm::vec3(x + std::cos(a) * radius, -.013f - t * t * .055f + std::sin(a) * radius * .75f,
                             (thumb ? .10f : .235f) + t * length);
        }));
    }
    return data;
}

float leadCoatRadius(float t) {
    const float shoulder = .10f * std::exp(-std::pow((t - .83f) / .12f, 2.0f));
    return (.255f + .18f * glm::smoothstep(.15f, .65f, t) + shoulder)
        * (1 - glm::smoothstep(.84f, 1.0f, t) * .78f);
}

float leadCoatFront(float x, float y) {
    const float t = (y + .14f) / 1.65f;
    const float chest = std::exp(-std::pow((t - .73f) / .23f, 2.0f));
    const float fraction = x / std::max(leadCoatRadius(t), .01f);
    return std::sqrt(std::max(.025f, 1 - fraction * fraction)) * (.23f + .085f * chest)
        - .025f * std::sin(t * pi);
}

glm::vec3 leadCoatShape(float u, float t) {
    const float a = u * 2 * pi;
    const float chest = std::exp(-std::pow((t - .73f) / .23f, 2.0f));
    const float fold = .011f * std::sin(a * 12 + t * 6) * (1 - t)
        + .005f * std::sin(a * 21 - t * 8);
    return {std::cos(a) * (leadCoatRadius(t) + fold), -.14f + t * 1.65f,
            std::sin(a) * (.23f + .085f * chest + fold) - .025f * std::sin(t * pi)};
}

glm::vec3 leadHatCenter(float t) {
    const float q = 1 - t;
    return 3 * q * q * t * glm::vec3(.42f, .95f, -.025f)
        + 3 * q * t * t * glm::vec3(1.12f, .90f, -.035f)
        + t * t * t * glm::vec3(1.02f, -.68f, .04f);
}

glm::vec3 leadHatShape(float u, float t) {
    const float a = u * 2 * pi;
    const glm::vec3 tangent = glm::normalize(leadHatCenter(t + .0005f) - leadHatCenter(t - .0005f));
    const glm::vec3 across = glm::normalize(glm::vec3(-tangent.y, tangent.x, 0));
    const glm::vec3 around = glm::cross(tangent, across);
    const float fullness = .285f * std::pow(std::max(0.0f, 1 - t), .72f) + .017f + .025f * std::sin(t * pi);
    const float fold = (.009f * std::sin(a * 4 + t * 4) + .003f * std::sin(a * 7 - t * 3))
        * std::sin(pi * t);
    return leadHatCenter(t) + (across * std::cos(a) + around * std::sin(a) * .90f) * (fullness + fold);
}

glm::vec3 leadSleeveShape(glm::vec3 shoulder, glm::vec3 elbow, glm::vec3 wrist, float u, float t) {
    const float a = u * 2 * pi;
    const auto center = [=](float v) {
        constexpr float joint = .51f;
        const bool upper = v < joint;
        const float f = upper ? v / joint : (v - joint) / (1 - joint);
        const glm::vec3 from = upper ? shoulder : elbow, to = upper ? elbow : wrist;
        const glm::vec3 elbowDirection = (wrist - shoulder) * .20f;
        const glm::vec3 start = upper ? (elbow - shoulder) * .90f : elbowDirection;
        const glm::vec3 end = upper ? elbowDirection : (wrist - elbow) * .90f;
        return (2*f*f*f-3*f*f+1)*from + (f*f*f-2*f*f+f)*start
            + (-2*f*f*f+3*f*f)*to + (f*f*f-f*f)*end;
    };
    const glm::vec3 p = center(t);
    const glm::vec3 tangent = glm::normalize(center(t + .0005f) - center(t - .0005f));
    const glm::vec3 across = glm::normalize(glm::cross(tangent, glm::vec3(0, 0, 1)));
    const glm::vec3 around = glm::cross(tangent, across);
    const float puff = .030f * std::exp(-std::pow((t - .17f) / .15f, 2.0f));
    const float folds = .008f * std::sin(a * 10) * std::exp(-std::pow((t - .18f) / .22f, 2.0f))
        + .003f * std::sin(t * 24 + a * 3);
    const float radius = glm::mix(.106f, .064f, t) + puff + folds;
    return p + (across * std::cos(a) + around * std::sin(a)) * radius;
}

// Only the leading dealer bends its cloth each frame. The CPU storage and GPU
// buffer remain allocated, and the static triangle indices never change.
void updateLeadSleeve(std::vector<float>& vertices, glm::vec3 shoulder, glm::vec3 elbow, glm::vec3 wrist) {
    constexpr int columns = 32, rows = 40;
    vertices.resize((columns + 1) * (rows + 1) * 8);
    std::size_t at = 0;
    for (int row = 0; row <= rows; ++row) {
        const float t = static_cast<float>(row) / rows;
        for (int column = 0; column <= columns; ++column) {
            const float u = static_cast<float>(column) / columns;
            const glm::vec3 p = leadSleeveShape(shoulder, elbow, wrist, u, t);
            const glm::vec3 du = leadSleeveShape(shoulder, elbow, wrist, u + .0005f, t)
                - leadSleeveShape(shoulder, elbow, wrist, u - .0005f, t);
            const glm::vec3 dv = leadSleeveShape(shoulder, elbow, wrist, u, t + .0005f)
                - leadSleeveShape(shoulder, elbow, wrist, u, t - .0005f);
            const glm::vec3 normal = glm::normalize(glm::cross(du, dv));
            for (const float value : {p.x, p.y, p.z, normal.x, normal.y, normal.z, u, t}) vertices[at++] = value;
        }
    }
}

using Pixel = std::array<unsigned char, 3>;
struct Bitmap {
    int width;
    int height;
    std::vector<unsigned char> data;
    Bitmap(int w, int h, Pixel fill) : width(w), height(h), data(static_cast<std::size_t>(w * h * 3)) {
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) set(x, y, fill);
    }
    void set(int x, int y, Pixel color) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        const auto at = static_cast<std::size_t>((y * width + x) * 3);
        for (int channel = 0; channel < 3; ++channel) data[at + channel] = color[channel];
    }
    void rectangle(int x, int y, int w, int h, Pixel color) {
        for (int py = y; py < y + h; ++py)
            for (int px = x; px < x + w; ++px) set(px, py, color);
    }
    void blend(int x, int y, Pixel color, float coverage) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        const auto at = static_cast<std::size_t>((y * width + x) * 3);
        for (int channel = 0; channel < 3; ++channel) {
            data[at + channel] = static_cast<unsigned char>(std::round(
                static_cast<float>(data[at + channel]) * (1.0f - coverage)
                + static_cast<float>(color[channel]) * coverage));
        }
    }
};

using Stroke = std::array<float, 4>;
std::vector<Stroke> glyph(char c) {
    switch (c) {
    case 'A': return {{0,6,2,0}, {2,0,4,6}, {.7f,4,3.3f,4}, {-.3f,6,.8f,6}, {3.2f,6,4.3f,6}};
    case '2': return {{0,1,1,0}, {1,0,3,0}, {3,0,4,1}, {4,1,4,2}, {4,2,0,6}, {0,6,4,6}};
    case '3': return {{0,0,4,0}, {4,0,2,2.7f}, {2,2.7f,3.3f,3}, {3.3f,3,4,4},
                      {4,4,4,5}, {4,5,3,6}, {3,6,1,6}, {1,6,0,5.3f}};
    case '4': return {{3,0,0,4}, {0,4,4.3f,4}, {3,0,3,6}, {2.2f,6,3.8f,6}};
    case '5': return {{4,0,0,0}, {0,0,0,2.7f}, {0,2.7f,3,2.7f}, {3,2.7f,4,3.7f},
                      {4,3.7f,4,5}, {4,5,3,6}, {3,6,1,6}, {1,6,0,5.3f}};
    case '6': return {{3.6f,0,2,0}, {2,0,.5f,1.5f}, {.5f,1.5f,0,3.8f}, {0,3.8f,0,5},
                      {0,5,1,6}, {1,6,3,6}, {3,6,4,5}, {4,5,4,3.6f}, {4,3.6f,3,2.8f}, {3,2.8f,.2f,3}};
    case '7': return {{0,0,4,0}, {4,0,1.1f,6}};
    case '8': return {{1,0,3,0}, {3,0,4,1}, {4,1,3.7f,2.1f}, {3.7f,2.1f,.4f,3.7f},
                      {.4f,3.7f,0,5}, {0,5,1,6}, {1,6,3,6}, {3,6,4,5}, {4,5,3.6f,3.7f},
                      {3.6f,3.7f,.3f,2.1f}, {.3f,2.1f,0,1}, {0,1,1,0}};
    case '9': return {{.4f,6,2,6}, {2,6,3.5f,4.5f}, {3.5f,4.5f,4,2.2f}, {4,2.2f,4,1},
                      {4,1,3,0}, {3,0,1,0}, {1,0,0,1}, {0,1,0,2.4f}, {0,2.4f,1,3.2f}, {1,3.2f,3.8f,3}};
    case '1': return {{.7f,1.3f,2.2f,0}, {2.2f,0,2.2f,6}, {.7f,6,3.7f,6}};
    case '0': return {{1,0,3,0}, {3,0,4,1.3f}, {4,1.3f,4,4.7f}, {4,4.7f,3,6},
                      {3,6,1,6}, {1,6,0,4.7f}, {0,4.7f,0,1.3f}, {0,1.3f,1,0}};
    case 'J': return {{1,0,4,0}, {3,0,3,4.8f}, {3,4.8f,2.2f,6}, {2.2f,6,.7f,6}, {.7f,6,0,4.8f}};
    case 'Q': return {{1,0,3,0}, {3,0,4,1.3f}, {4,1.3f,4,4.7f}, {4,4.7f,3,6},
                      {3,6,1,6}, {1,6,0,4.7f}, {0,4.7f,0,1.3f}, {0,1.3f,1,0}, {2.5f,4.4f,4.4f,6.4f}};
    case 'K': return {{.3f,0,.3f,6}, {.3f,3.4f,4,0}, {1.6f,2.3f,4,6}, {-.2f,0,.8f,0}, {-.2f,6,.8f,6}};
    default: return {};
    }
}

void drawText(Bitmap& bitmap, const std::string& text, int left, int top,
              int scale, Pixel color, bool rotate180 = false) {
    int cursor = left;
    for (char c : text) {
        const auto shape = glyph(c);
        const float weight = static_cast<float>(scale) * .27f;
        for (int py = top - scale; py < top + 8 * scale; ++py) {
            for (int px = cursor - scale; px < cursor + 6 * scale; ++px) {
                float distance = 1000.0f;
                const glm::vec2 point(static_cast<float>(px) + .5f, static_cast<float>(py) + .5f);
                for (const auto& stroke : shape) {
                    const glm::vec2 a(static_cast<float>(cursor) + stroke[0] * static_cast<float>(scale),
                                      static_cast<float>(top) + stroke[1] * static_cast<float>(scale));
                    const glm::vec2 b(static_cast<float>(cursor) + stroke[2] * static_cast<float>(scale),
                                      static_cast<float>(top) + stroke[3] * static_cast<float>(scale));
                    const glm::vec2 delta = b - a;
                    const float t = std::clamp(glm::dot(point - a, delta) / glm::dot(delta, delta), 0.0f, 1.0f);
                    distance = std::min(distance, glm::length(point - (a + delta * t)));
                }
                const float coverage = std::clamp(weight + .5f - distance, 0.0f, 1.0f);
                if (coverage > 0)
                    bitmap.blend(rotate180 ? bitmap.width - 1 - px : px,
                                 rotate180 ? bitmap.height - 1 - py : py, color, coverage);
            }
        }
        cursor += 6 * scale;
    }
}

void drawSuit(Bitmap& bitmap, int suit, float cx, float cy, float size,
              Pixel color, bool rotate180 = false) {
    const int minX = static_cast<int>(cx - size);
    const int maxX = static_cast<int>(cx + size);
    const int minY = static_cast<int>(cy - size);
    const int maxY = static_cast<int>(cy + size);
    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            const float x = (static_cast<float>(px) - cx) / size;
            const float y = (static_cast<float>(py) - cy) / size;
            bool inside = false;
            if (suit == 1) { // Diamond.
                inside = std::abs(x) + std::abs(y) * .75f < .78f;
            } else if (suit == 2) { // Heart: two lobes and a pointed lower half.
                inside = ((x + .33f) * (x + .33f) + (y + .30f) * (y + .30f) < .22f)
                    || ((x - .33f) * (x - .33f) + (y + .30f) * (y + .30f) < .22f)
                    || (y >= -.27f && y <= .83f && std::abs(x) < (.83f - y) * .71f);
            } else if (suit == 3) { // Spade.
                inside = ((x + .32f) * (x + .32f) + (y - .12f) * (y - .12f) < .20f)
                    || ((x - .32f) * (x - .32f) + (y - .12f) * (y - .12f) < .20f)
                    || (y >= -.85f && y <= .12f && std::abs(x) < (y + .85f) * .71f)
                    || (y > .15f && y < .83f && std::abs(x) < .12f + std::max(0.0f, y - .55f));
            } else { // Club.
                inside = (x * x + (y + .42f) * (y + .42f) < .18f)
                    || ((x + .38f) * (x + .38f) + (y - .08f) * (y - .08f) < .18f)
                    || ((x - .38f) * (x - .38f) + (y - .08f) * (y - .08f) < .18f)
                    || (y > .05f && y < .83f && std::abs(x) < .12f + std::max(0.0f, y - .55f));
            }
            if (inside) bitmap.set(rotate180 ? bitmap.width - 1 - px : px,
                                   rotate180 ? bitmap.height - 1 - py : py, color);
        }
    }
}

Bitmap cardBitmap(int rank, int suit) {
    Bitmap bitmap(160, 224, {243, 237, 215});
    const Pixel ink = (suit == 1 || suit == 2) ? Pixel{176, 43, 46} : Pixel{26, 38, 43};
    const Pixel border{208, 197, 165};
    bitmap.rectangle(5, 5, 150, 2, border);
    bitmap.rectangle(5, 217, 150, 2, border);
    bitmap.rectangle(5, 5, 2, 214, border);
    bitmap.rectangle(153, 5, 2, 214, border);
    const std::string value = rank == 1 ? "A" : rank == 11 ? "J" : rank == 12 ? "Q"
        : rank == 13 ? "K" : std::to_string(rank);
    drawText(bitmap, value, 13, 14, 4, ink);
    drawSuit(bitmap, suit, 22, 55, 11, ink);
    drawText(bitmap, value, 13, 14, 4, ink, true);
    drawSuit(bitmap, suit, 22, 55, 11, ink, true);

    if (rank > 10 || rank == 1) {
        drawSuit(bitmap, suit, 80, rank == 1 ? 111.0f : 128.0f, rank == 1 ? 45.0f : 32.0f, ink);
        if (rank > 10) {
            drawText(bitmap, value, 66, 50, 6, ink);
            const Pixel ornament{194, 166, 93};
            bitmap.rectangle(49, 163, 62, 2, ornament);
            bitmap.rectangle(57, 169, 46, 1, ornament);
        }
    } else {
        // Deliberately spacious pip grid, recognizable from either side of the table.
        const int leftCount = rank / 2;
        const int rightCount = rank - leftCount;
        for (int column = 0; column < 2; ++column) {
            const int count = column == 0 ? leftCount : rightCount;
            for (int i = 0; i < count; ++i) {
                const float y = count == 1 ? 112.0f
                    : 57.0f + 110.0f * static_cast<float>(i) / static_cast<float>(count - 1);
                drawSuit(bitmap, suit, column == 0 ? 58.0f : 102.0f, y, 14.0f, ink);
            }
        }
    }
    // OpenGL's V origin is bottom-left; flip rows after drawing in image coordinates.
    for (int y = 0; y < bitmap.height / 2; ++y)
        for (int x = 0; x < bitmap.width * 3; ++x)
            std::swap(bitmap.data[static_cast<std::size_t>(y * bitmap.width * 3 + x)],
                      bitmap.data[static_cast<std::size_t>((bitmap.height - 1 - y) * bitmap.width * 3 + x)]);
    return bitmap;
}

Bitmap cardBackBitmap() {
    Bitmap bitmap(160, 224, {231, 220, 187});
    bitmap.rectangle(7, 7, 146, 210, {58, 24, 83});
    for (int y = 12; y < 212; ++y) {
        for (int x = 12; x < 148; ++x) {
            const int diagonal = (x + y) % 18;
            const int otherDiagonal = (x - y + 224) % 18;
            if (diagonal < 2 || otherDiagonal < 2) bitmap.set(x, y, {126, 60, 117});
        }
    }
    bitmap.rectangle(12, 12, 136, 2, {181, 149, 78});
    bitmap.rectangle(12, 210, 136, 2, {181, 149, 78});
    bitmap.rectangle(12, 12, 2, 200, {181, 149, 78});
    bitmap.rectangle(146, 12, 2, 200, {181, 149, 78});
    drawSuit(bitmap, 1, 80, 112, 38, {221, 181, 89});
    return bitmap;
}

Bitmap feltBitmap() {
    Bitmap bitmap(128, 128, {255, 255, 255});
    std::uint32_t noise = 0x12345678;
    for (int y = 0; y < bitmap.height; ++y) {
        for (int x = 0; x < bitmap.width; ++x) {
            noise ^= noise << 13;
            noise ^= noise >> 17;
            noise ^= noise << 5;
            const int weave = ((x + y) % 3 == 0) ? 6 : 0;
            const auto value = static_cast<unsigned char>(211 + (noise % 32) + weave);
            bitmap.set(x, y, {value, value, value});
        }
    }
    return bitmap;
}

Bitmap chipBitmap() {
    Bitmap bitmap(128, 128, {255, 255, 255});
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const float dx = (static_cast<float>(x) - 63.5f) / 63.5f;
            const float dy = (static_cast<float>(y) - 63.5f) / 63.5f;
            const float radius = std::sqrt(dx * dx + dy * dy);
            const float angle = std::atan2(dy, dx);
            const bool edgeMark = radius > .75f && (static_cast<int>((angle + pi) / (pi / 6.0f)) % 2 == 0);
            const bool ring = std::abs(radius - .64f) < .025f || std::abs(radius - .57f) < .018f;
            if (edgeMark || ring) bitmap.set(x, y, {236, 220, 176});
            else if (radius < .52f) bitmap.set(x, y, {185, 185, 185});
        }
    }
    return bitmap;
}

struct Textures {
    std::vector<GLuint> owned;
    ~Textures() {
        if (!owned.empty()) glDeleteTextures(static_cast<GLsizei>(owned.size()), owned.data());
    }
    GLuint upload(const Bitmap& bitmap, bool repeat = false) {
        GLuint id = 0;
        glGenTextures(1, &id);
        owned.push_back(id);
        glBindTexture(GL_TEXTURE_2D, id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, bitmap.width, bitmap.height, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, bitmap.data.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        return id;
    }
};

glm::mat4 transform(glm::vec3 position, glm::vec3 scale, float rotation = 0.0f) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, rotation, glm::vec3(0, 1, 0));
    return glm::scale(model, scale);
}

} // namespace

struct Renderer::Impl {
    Program program;
    Program skyProgram;
    Mesh cube;
    Mesh plane;
    Mesh cylinder;
    Mesh sphere;
    Mesh horn;
    Mesh ring;
    Mesh arch;
    Mesh mask;
    Mesh jacket;
    Mesh ruff;
    Mesh hood;
    Mesh curtain;
    Mesh sleeve;
    Mesh glove;
    Mesh leadCoat;
    Mesh leadHat;
    Mesh leadCap;
    Mesh leadLace;
    Mesh leadJabot;
    Mesh leadTrim;
    Mesh leadMaskTrim;
    Mesh leadMask;
    Mesh leadNeck;
    Mesh leadLeg;
    Mesh leadBoot;
    Mesh leadSleeve;
    std::vector<float> leadSleeveVertices;
    Mesh sky;
    Textures textures;
    std::array<ArtTexture, 3> deckArt;
    int activeDeck = 0;
    std::array<GLuint, 52> cardFaces{};
    GLuint white = 0;
    GLuint felt = 0;
    GLuint back = 0;
    GLuint chip = 0;
    struct Locations {
        GLint model, view, projection, normal, color, uvScale, camera, light,
              intensity, shininess, specular, emission, fogColor, opacity, velvet, roomLight,
              viewport, lampBrightness, material, useTexture, lighting;
    } u{};
    GLint skyResolution = -1;
    bool texturesEnabled = true;

    explicit Impl(const std::filesystem::path& root) {
        program.load(root / "shaders" / "table.vert", root / "shaders" / "table.frag");
        skyProgram.load(root / "shaders" / "sky.vert", root / "shaders" / "sky.frag");
        cube = makeCube();
        plane = makeTopPlane();
        cylinder = makeCylinder(80);
        sphere = makeSphere(24, 16);
        horn = makeHorn();
        ring = makeRing(64, false);
        arch = makeRing(48, true);
        mask = surfaceMesh(surfaceData(96, 96, porcelainShape));
        jacket = surfaceMesh(surfaceData(72, 64, jacketShape, true));
        ruff = surfaceMesh(surfaceData(128, 12, [](float u, float v) {
            const float a = u * 2 * pi, radius = .19f + v * .39f;
            return glm::vec3(std::cos(a) * radius, .02f - v * .09f + std::sin(a * 24) * v * .065f,
                             std::sin(a) * radius * .70f);
        }));
        hood = surfaceMesh(surfaceData(64, 48, [](float u, float v) {
            const float a = u * 2 * pi;
            const float y = -.49f + v * .98f;
            const float width = .13f + .25f * std::sin(v * pi);
            const float folds = .013f * std::cos(a * 12 + v * 3);
            return glm::vec3(std::cos(a) * (width + folds), y,
                             std::sin(a) * (.24f + folds) - .035f);
        }, true));
        curtain = surfaceMesh(surfaceData(100, 64, [](float u, float v) {
            const float gather = .12f * std::sin(v * pi);
            const float x = (u * 2 - 1) * (1.65f - gather);
            const float fold = .12f * std::sin(u * pi * 20) + .025f * std::sin(u * pi * 41 + v * 2);
            return glm::vec3(x, -.75f + v * 5.1f, fold + .22f * std::sin(v * pi));
        }));
        sleeve = surfaceMesh(sleeveSurface({.52f, 1.25f, -2.95f}, {.73f, .70f, -2.70f}, {.86f, .17f, -2.40f}));
        glove = surfaceMesh(gloveSurface());
        leadCoat = surfaceMesh(surfaceData(80, 64, leadCoatShape, true));
        leadMask = surfaceMesh(surfaceData(80, 80, leadPorcelainShape));
        leadNeck = surfaceMesh(surfaceData(40, 20, [](float u, float v) {
            const float a = u * 2 * pi;
            return glm::vec3(std::cos(a) * (.084f + .006f * std::sin(v * pi)), v * .17f,
                             std::sin(a) * (.068f + .005f * std::sin(v * pi)) + .012f * v);
        }, true));
        leadLeg = surfaceMesh(surfaceData(32, 36, [](float u, float t) {
            const float a = u * 2 * pi;
            const float fold = .004f * std::sin(a * 8 + t * 7);
            const float knee = .015f * std::exp(-std::pow((t - .52f) / .13f, 2.0f));
            return glm::vec3(std::cos(a) * (.13f - t * .045f + fold), -.04f - t * .61f,
                std::sin(a) * (.14f - t * .055f + fold) + knee);
        }));
        leadBoot = surfaceMesh(surfaceData(40, 32, [](float u, float t) {
            const float a = u * 2 * pi;
            const float profile = std::pow(std::max(0.0f, std::sin(t * pi)), .45f);
            return glm::vec3(std::cos(a) * .105f * profile,
                .014f + std::sin(a) * .058f * profile + .025f * std::exp(-std::pow((t-.35f)/.30f,2.0f)),
                -.075f + t * .34f);
        }));
        leadHat = surfaceMesh(surfaceData(40, 64, leadHatShape));
        leadCap = surfaceMesh(surfaceData(64, 32, [](float u, float v) {
            const float a = u * 2 * pi;
            const float dome = std::sqrt(std::max(.001f, 1 - v * v));
            const float fold = .008f * std::sin(a * 12 + v * 3);
            return glm::vec3(std::cos(a) * (.36f * dome + .02f + fold), .28f + .36f * v,
                             std::sin(a) * (.24f * dome + .018f + fold) - .10f);
        }, true));
        leadLace = surfaceMesh(surfaceData(192, 20, [](float u, float v) {
            const float a = u * 2 * pi;
            const float scallop = .028f * std::cos(a * 24) * v * v;
            const float radius = .17f + v * .47f + scallop;
            return glm::vec3(std::cos(a) * radius,
                .015f - v * .12f + std::sin(a * 24) * .026f * std::sin(v * pi),
                std::sin(a) * radius * .67f);
        }));
        leadJabot = surfaceMesh(surfaceData(64, 40, [](float u, float v) {
            const float side = u * 2 - 1;
            const float x = side * (.045f + v * .225f);
            const float y = .76f + v * .61f - .035f * (1 - side * side);
            return glm::vec3(x, y, leadCoatFront(x, y) + .026f
                + .014f * std::cos(side * pi * 6) * std::sin(v * pi));
        }));
        SurfaceData trim;
        const auto frontPoint = [](float x, float y) { return glm::vec3(x, y, leadCoatFront(x, y) + .022f); };
        for (const float side : {-1.0f, 1.0f}) {
            for (const float edge : {-1.0f, 1.0f})
                appendSurface(trim, cordSurface(48, [=](float t) {
                    return frontPoint(side * (.035f + .29f * t + edge * (.025f + .04f * t)), .62f + t * .68f);
                }, .009f));
            // Sewn gold scrolls curl out from the waist clasp and the lapels.
            for (int scroll = 0; scroll < 3; ++scroll)
                appendSurface(trim, cordSurface(48, [=](float t) {
                    const float a = t * 2 * pi * 1.25f;
                    const float r = .075f * (1 - .72f * t);
                    const float x = side * (.12f + static_cast<float>(scroll) * .085f + std::cos(a) * r);
                    const float y = .46f + static_cast<float>(scroll % 2) * .05f + std::sin(a) * r * .7f;
                    return frontPoint(x, y);
                }, .0065f));
            appendSurface(trim, cordSurface(48, [=](float t) {
                const float a = t * 2 * pi * 1.3f, r = .065f * (1 - t * .75f);
                return frontPoint(side * (.20f + std::cos(a) * r), 1.05f + std::sin(a) * r * 1.3f);
            }, .0065f));
        }
        for (const float y : {.32f, .50f})
            appendSurface(trim, cordSurface(80, [=](float t) {
                const float x = (t * 2 - 1) * .30f;
                return frontPoint(x, y);
            }, .008f));
        for (const float y : {.43f, 1.37f})
            appendSurface(trim, cordSurface(64, [=](float t) {
                const float a = t * 2 * pi;
                const float x = std::cos(a) * (y > 1 ? .070f : .105f);
                return frontPoint(x, y + std::sin(a) * (y > 1 ? .090f : .075f));
            }, .012f));
        leadTrim = surfaceMesh(trim);
        SurfaceData faceTrim;
        const auto onFace = [](float x, float y) {
            return leadPorcelainShape((x + 1) * .5f, (y + 1) * .5f) + glm::vec3(0, 0, .008f);
        };
        appendSurface(faceTrim, cordSurface(80, [=](float t) {
            const float a = t * 2 * pi;
            return onFace(.39f + std::cos(a) * .26f, .20f + std::sin(a) * .13f);
        }, .0035f));
        for (int curl = 0; curl < 3; ++curl)
            appendSurface(faceTrim, cordSurface(48, [=](float t) {
                const float a = t * 2 * pi * 1.15f;
                const float r = .10f * (1 - t * .78f);
                return onFace(.64f + static_cast<float>(curl) * .025f + std::cos(a) * r,
                    .30f - static_cast<float>(curl) * .20f + std::sin(a) * r);
            }, .0023f));
        leadMaskTrim = surfaceMesh(faceTrim);
        const glm::vec3 shoulder(.53f, 1.23f, -2.705f), elbow(.88f, .66f, -2.58f), wrist(1.04f, .14f, -2.35f);
        const auto firstSleeve = surfaceData(32, 40, [=](float u, float t) {
            return leadSleeveShape(shoulder, elbow, wrist, u, t);
        });
        leadSleeve = surfaceMesh(firstSleeve);
        leadSleeveVertices = firstSleeve.vertices;
        sky = makeMesh({-1, -1, 0, 0, 0, 1, 0, 0,
                         3, -1, 0, 0, 0, 1, 2, 0,
                        -1,  3, 0, 0, 0, 1, 0, 2}, {0, 1, 2});
        skyResolution = glGetUniformLocation(skyProgram.id, "uResolution");
        white = textures.upload(Bitmap(1, 1, {255, 255, 255}));
        felt = textures.upload(feltBitmap(), true);
        back = textures.upload(cardBackBitmap());
        chip = textures.upload(chipBitmap());
        const std::array<const char*, 3> artNames{{"deck-hands.tga", "deck-life.tga", "deck-coins.tga"}};
        for (std::size_t i = 0; i < deckArt.size(); ++i) {
            try { deckArt[i].load(root / "art" / artNames[i], true); }
            catch (const std::exception& error) { std::cerr << "Deck artwork fallback: " << error.what() << '\n'; }
        }
        for (int suit = 0; suit < 4; ++suit)
            for (int rank = 1; rank <= 13; ++rank)
                cardFaces[static_cast<std::size_t>(suit * 13 + rank - 1)] = textures.upload(cardBitmap(rank, suit));
        const auto uniform = [&](const char* name) { return glGetUniformLocation(program.id, name); };
        u.model = uniform("uModel");
        u.view = uniform("uView");
        u.projection = uniform("uProjection");
        u.normal = uniform("uNormalMatrix");
        u.color = uniform("uColor");
        u.uvScale = uniform("uUvScale");
        u.camera = uniform("uCameraPosition");
        u.light = uniform("uLightPosition");
        u.intensity = uniform("uLightIntensity");
        u.shininess = uniform("uShininess");
        u.specular = uniform("uSpecular");
        u.emission = uniform("uEmission");
        u.fogColor = uniform("uFogColor");
        u.opacity = uniform("uOpacity");
        u.velvet = uniform("uVelvet");
        u.roomLight = uniform("uRoomLight");
        u.viewport = uniform("uViewport");
        u.lampBrightness = uniform("uLampBrightness");
        u.material = uniform("uMaterial");
        u.useTexture = uniform("uUseTexture");
        u.lighting = uniform("uLighting");
        glUseProgram(program.id);
        glUniform1i(uniform("uTexture"), 0);
        glUseProgram(0);
    }

    void draw(const Mesh& mesh, const glm::mat4& model, glm::vec3 color,
              GLuint texture = 0, glm::vec2 uvScale = glm::vec2(1.0f),
              float specular = .08f, float shininess = 32.0f, float emission = 0.0f,
              float opacity = 1.0f, float velvet = 0.0f, int material = 0) {
        const glm::mat3 normal = glm::transpose(glm::inverse(glm::mat3(model)));
        glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix3fv(u.normal, 1, GL_FALSE, glm::value_ptr(normal));
        glUniform3fv(u.color, 1, glm::value_ptr(color));
        glUniform2fv(u.uvScale, 1, glm::value_ptr(uvScale));
        glUniform1f(u.shininess, shininess);
        glUniform1f(u.specular, specular);
        glUniform1f(u.emission, emission);
        glUniform1f(u.opacity, opacity);
        glUniform1f(u.velvet, velvet);
        glUniform1i(u.material, material);
        glUniform1i(u.useTexture, texturesEnabled && texture != 0 ? GL_TRUE : GL_FALSE);
        glBindTexture(GL_TEXTURE_2D, texture ? texture : white);
        mesh.draw();
    }

    GLuint cardBack() const {
        const auto& art = deckArt[static_cast<std::size_t>(activeDeck)];
        return art ? art.id() : back;
    }

    void beam(glm::vec3 from, glm::vec3 to, float radius, glm::vec3 color,
              const glm::mat4& root = glm::mat4(1)) {
        const glm::vec3 delta = to - from;
        const float length = glm::length(delta);
        if (length < .0001f) return;
        const glm::vec3 direction = delta / length;
        const glm::vec3 axis = glm::cross(glm::vec3(0, 1, 0), direction);
        glm::mat4 model = glm::translate(glm::mat4(1), (from + to) * .5f);
        if (glm::length(axis) > .0001f) {
            model = glm::rotate(model, std::acos(std::clamp(direction.y, -1.0f, 1.0f)),
                                glm::normalize(axis));
        } else if (direction.y < 0) {
            model = glm::rotate(model, pi, glm::vec3(1, 0, 0));
        }
        draw(cylinder, root * glm::scale(model, {radius, length, radius}), color,
             0, glm::vec2(1), .25f);
    }

    void diamond(glm::vec3 position, float size, glm::vec3 color,
                 const glm::mat4& root = glm::mat4(1)) {
        glm::mat4 model = glm::translate(glm::mat4(1), position);
        model = glm::rotate(model, pi * .25f, glm::vec3(0, 0, 1));
        draw(cube, root * glm::scale(model, {size, size, .025f}), color);
    }

    void drawSky(int width, int height) {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glUseProgram(skyProgram.id);
        glUniform2f(skyResolution, static_cast<float>(width), static_cast<float>(height));
        sky.draw();
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    void drawCasino(float time, bool animate) {
        const glm::vec3 stone{.40f, .22f, .35f};
        const glm::vec3 plum{.29f, .10f, .38f};
        const glm::vec3 gold{.78f, .53f, .24f};
        const glm::vec3 crimson{.55f, .13f, .23f};
        const glm::vec3 trim{.62f, .46f, .35f};
        const glm::vec3 window{.96f, .55f, .22f};
        constexpr float streetAngle = -.70f;
        glm::mat4 building = glm::translate(glm::mat4(1), {2.2f, 0, -3.0f});
        building = glm::rotate(building, streetAngle, glm::vec3(0, 1, 0));
        building = glm::scale(building, {1.35f, 1.12f, 1.12f});
        const auto block = [&](glm::vec3 position, glm::vec3 size, glm::vec3 color,
                               float emission = 0.0f) {
            draw(cube, building * transform(position, size), color, 0,
                 glm::vec2(1), .08f, 32, emission);
        };

        // A diagonal street and shallow forecourt connect the facade to the evening scene.
        draw(cube, transform({0, -.18f, -8}, {90, .30f, 100}), {.16f, .13f, .15f});
        // The road, curb and forecourt share a basis: their edges meet and stay parallel.
        constexpr float streetCenter = 5.55f;
        constexpr float streetWidth = 3.25f;
        block({.6f, -.006f, streetCenter}, {60, .035f, streetWidth}, {.075f, .068f, .095f});
        for (const float side : {-1.0f, 1.0f})
            block({.6f, .037f, streetCenter + side * streetWidth * .5f},
                  {60, .055f, .15f}, {.38f, .29f, .31f});
        for (int i = -18; i < 19; ++i)
            block({static_cast<float>(i) * 1.65f, .016f, streetCenter},
                  {.83f, .015f, .055f}, {.59f, .46f, .28f});
        block({.6f, .025f, 2.55f}, {9.2f, .08f, 2.6f}, {.27f, .22f, .25f});
        for (int i = -4; i <= 5; ++i)
            block({static_cast<float>(i) * .92f, .07f, 2.55f}, {.016f, .012f, 2.55f}, {.20f, .16f, .20f});

        // A modest two-storey casino, with a lower annex and flat cornices.
        block({0, .15f, 0}, {5.65f, .30f, 3.70f}, {.34f, .29f, .28f});
        block({0, 1.98f, 0}, {5.25f, 3.60f, 3.20f}, stone);
        block({0, .54f, 1.63f}, {5.30f, .65f, .10f}, plum);
        block({0, 2.05f, 1.66f}, {5.40f, .12f, .13f}, trim);
        block({0, 3.52f, 1.65f}, {5.38f, .40f, .14f}, plum);
        block({0, 3.86f, 0}, {5.65f, .20f, 3.65f}, crimson);
        block({0, 3.99f, 0}, {5.72f, .065f, 3.71f}, gold);
        block({0, 4.08f, -.1f}, {5.34f, .14f, 3.25f}, plum);
        for (const float x : {-2.48f, 2.48f})
            block({x, 1.96f, 1.69f}, {.17f, 3.43f, .19f}, trim);

        block({3.65f, .13f, -.22f}, {2.55f, .26f, 3.33f}, {.30f, .25f, .27f});
        block({3.65f, 1.40f, -.22f}, {2.35f, 2.55f, 3.08f}, {.34f, .19f, .32f});
        block({3.65f, 2.73f, -.22f}, {2.61f, .16f, 3.36f}, plum);
        block({3.65f, 2.84f, -.22f}, {2.66f, .055f, 3.42f}, gold);
        block({3.65f, .56f, 1.35f}, {2.40f, .59f, .10f}, plum);
        block({3.65f, 1.92f, 1.35f}, {2.45f, .085f, .13f}, trim);

        // Recessed windows are lit from within; no street or interior characters.
        const auto frontWindow = [&](float x, float y, float z, float width, float height) {
            block({x, y, z}, {width + .12f, height + .12f, .10f}, trim);
            block({x, y, z + .065f}, {width, height, .045f}, window, .55f);
            block({x, y, z + .10f}, {.035f, height + .01f, .035f}, plum);
            block({x, y, z + .10f}, {width + .01f, .035f, .035f}, plum);
        };
        for (const float x : {-1.63f, 1.63f}) {
            frontWindow(x, 1.23f, 1.67f, .63f, 1.02f);
            frontWindow(x, 2.73f, 1.67f, .63f, .90f);
        }
        for (const float x : {3.08f, 4.20f}) frontWindow(x, 1.25f, 1.36f, .56f, .95f);
        for (const float z : {-.94f, .38f}) {
            block({4.845f, 1.25f, z}, {.10f, 1.07f, .69f}, trim);
            block({4.91f, 1.25f, z}, {.045f, .95f, .57f}, window, .45f);
            block({4.94f, 1.25f, z}, {.03f, .98f, .035f}, plum);
            block({4.94f, 1.25f, z}, {.03f, .035f, .60f}, plum);
        }

        // A red double door and restrained three-bell emblem carry the jester theme.
        block({0, 1.10f, 1.69f}, {1.43f, 1.83f, .15f}, trim);
        block({0, 1.10f, 1.79f}, {1.23f, 1.66f, .085f}, plum);
        for (const float side : {-1.0f, 1.0f}) {
            block({side * .30f, 1.08f, 1.85f}, {.55f, 1.56f, .075f}, crimson);
            block({side * .30f, 1.40f, 1.90f}, {.40f, .63f, .035f}, {.62f, .37f, .22f}, .20f);
            draw(sphere, building * transform({side * .09f, .98f, 1.94f}, {.035f, .035f, .025f}), gold);
        }
        block({0, .22f, 2.02f}, {1.90f, .16f, .76f}, trim);
        block({0, .11f, 2.36f}, {2.17f, .12f, .68f}, {.40f, .33f, .31f});
        block({0, 2.89f, 1.71f}, {1.63f, .81f, .12f}, gold);
        block({0, 2.89f, 1.79f}, {1.48f, .66f, .05f}, plum);
        diamond({0, 2.77f, 1.84f}, .28f, crimson, building);
        for (int i = -1; i <= 1; ++i) {
            const float x = static_cast<float>(i) * .37f;
            beam({0, 2.80f, 1.87f}, {x, 3.09f, 1.87f}, .024f, gold, building);
            draw(sphere, building * transform({x, 3.10f, 1.87f}, {.07f, .07f, .045f}), gold,
                 0, glm::vec2(1), .2f, 32, .10f);
        }
        for (const float x : {-3.10f, 5.28f}) {
            const float pulse = animate ? .025f * std::sin(time * 1.4f + x) : 0.0f;
            draw(cylinder, building * transform({x, 1.0f, 2.1f}, {.042f, 2.0f, .042f}), {.30f, .23f, .24f});
            block({x, 2.05f, 2.1f}, {.25f, .39f, .25f}, window, .55f + pulse);
            block({x, 2.29f, 2.1f}, {.34f, .08f, .34f}, plum);
        }
    }

#include "RendererRoom.inl"

    void drawDealer(const Scene& scene, bool animate) {
        const glm::vec3 black{.025f, .030f, .033f}, crimson{.38f, .020f, .033f};
        const glm::vec3 gold{.62f, .42f, .16f}, ivory{.90f, .845f, .75f};
        const float breath = animate ? std::sin(scene.time * .65f) * .005f : 0;
        constexpr float z = -3.68f;
        const glm::mat4 body = transform({0, -.32f + breath, z}, {.86f, 1.02f, .92f});
        // The waist is near the tabletop; the hips, knees and planted boots
        // continue all the way to the room floor behind its far edge.
        for (const float side : {-1.0f, 1.0f}) {
            const glm::mat4 leg = transform({side * .18f, 0, z + .015f}, {1, 1, 1}, side * .035f);
            draw(leadLeg, leg, black, felt, {3, 5}, .045f, 24, 0, 1, .35f, 2);
            draw(leadBoot, transform({side * .18f, -.740f, z + .015f}, {1, 1, 1}, side * .035f),
                 {.020f, .018f, .020f}, 0, glm::vec2(1), .16f, 48);
        }
        draw(leadCoat, body, crimson, felt, {4, 4}, .12f, 32, 0, 1, 1, 9);
        draw(leadJabot, body, {.78f, .735f, .64f}, 0, glm::vec2(1), .06f, 20, 0, 1, .35f, 7);
        draw(leadTrim, body, gold, 0, glm::vec2(1), .58f, 84);
        draw(sphere, body * transform({0, 1.37f, leadCoatFront(0, 1.37f) + .045f}, {.034f, .049f, .023f}),
             {.27f, .015f, .032f}, 0, glm::vec2(1), .85f, 120);
        draw(ruff, body * transform({0, 1.43f, .015f}, {.65f, .35f, .65f}),
             {.40f, .27f, .13f}, 0, glm::vec2(1), .30f, 48);
        draw(leadLace, body * transform({0, 1.465f, .015f}, {.70f, .80f, .72f}),
             {.80f, .755f, .66f}, 0, glm::vec2(1), .07f, 24, 0, 1, .45f, 7);
        draw(leadNeck, transform({0, 1.18f + breath, z - .015f}, {1, 1, 1}),
             black, felt, {2, 2}, .06f, 28, 0, 1, .35f, 2);
        const float tilt = animate ? std::sin(scene.time * .33f) * .012f : 0;
        glm::mat4 head = glm::translate(glm::mat4(1), {0, 1.54f + breath, z + .015f});
        head = glm::rotate(head, tilt, glm::vec3(0, 0, 1));
        head = glm::rotate(head, -.12f, glm::vec3(1, 0, 0));
        draw(hood, head * transform({0, 0, -.10f}, {.45f, .62f, .52f}),
             black, felt, {4, 3}, .08f, 16, 0, 1, .8f, 2);
        draw(leadMask, head, ivory, 0, glm::vec2(1), .28f, 76, .02f, 1, 0, 8);
        draw(leadMaskTrim, head, gold, 0, glm::vec2(1), .62f, 96);
        draw(leadCap, head * glm::scale(glm::mat4(1), {.49f, .52f, .52f}),
             black, felt, {3, 3}, .045f, 18, 0, 1, 1, 5);
        draw(ring, head * transform({0, .19f, -.052f}, {.175f, .007f, .125f}), crimson,
             0, glm::vec2(1), .22f, 48, 0, 1, .4f, 6);
        for (const float side : {-1.0f, 1.0f}) {
            const float scale = side < 0 ? 1.02f : .94f;
            const float sway = animate ? std::sin(scene.time * .47f + side) * .018f : 0;
            glm::mat4 h = head * glm::translate(glm::mat4(1), {side * .06f, .23f, -.078f});
            h = glm::rotate(h, sway, glm::vec3(0, 0, 1));
            h = glm::scale(h, {side * scale * .47f, scale * .58f, scale * .50f});
            draw(leadHat, h, side < 0 ? glm::vec3(.042f, .053f, .056f) : crimson,
                 felt, {3, 4}, .045f, 18, 0, 1, 1, 5);
            const glm::vec3 bell = glm::vec3(h * glm::vec4(leadHatCenter(1), 1));
            draw(sphere, transform(bell, {.024f, .029f, .024f}), gold, 0, glm::vec2(1), .7f, 110);
            draw(cube, transform(bell + glm::vec3(0, -.004f, .024f), {.033f, .004f, .004f}), {.025f, .018f, .025f});
        }

        float activeReach = std::clamp(scene.dealerReach, 0.0f, 1.0f);
        for (const auto* cards : {&scene.player, &scene.dealer})
            for (const auto& card : *cards)
                if (card.arrival < 1) activeReach = std::max(activeReach, std::sin(pi * std::clamp(card.arrival, 0.0f, 1.0f)));
        for (const float side : {-1.0f, 1.0f}) {
            const float reach = ((side > 0) == scene.dealingToPlayer) ? activeReach : 0;
            const glm::vec3 shoulder(side * .29f, 1.05f + breath, z + .015f);
            const glm::vec3 elbow = glm::mix(glm::vec3(side * .62f, .50f, -3.41f),
                                            glm::vec3(side * .63f, .65f, -3.24f), reach);
            const glm::vec3 wrist = glm::mix(glm::vec3(side * .83f, .082f, -2.89f),
                                            glm::vec3(side * .75f, .33f, -2.49f), reach);
            updateLeadSleeve(leadSleeveVertices, shoulder, elbow, wrist);
            leadSleeve.update(leadSleeveVertices);
            draw(leadSleeve, glm::mat4(1), crimson, felt, {4, 3}, .10f, 30, 0, 1, .8f, 10);
            const glm::vec3 axis = glm::normalize(wrist - elbow);
            const glm::vec3 rotationAxis = glm::cross(glm::vec3(0, 1, 0), axis);
            glm::mat4 cuff = glm::translate(glm::mat4(1), wrist);
            if (glm::length(rotationAxis) > .0001f)
                cuff = glm::rotate(cuff, std::acos(std::clamp(axis.y, -1.0f, 1.0f)), glm::normalize(rotationAxis));
            draw(cylinder, glm::scale(cuff, {.082f, .080f, .082f}), crimson,
                 0, glm::vec2(1), .12f, 32, 0, 1, .4f, 6);
            draw(leadLace, glm::scale(cuff, {.21f, .35f, .21f}), {.78f, .725f, .63f},
                 0, glm::vec2(1), .08f, 24, 0, 1, .5f, 7);
            glm::mat4 hand = glm::translate(glm::mat4(1), wrist + glm::vec3(0, 0, .015f));
            hand = glm::rotate(hand, side * .11f, glm::vec3(0, 1, 0));
            hand = glm::rotate(hand, -reach * .17f, glm::vec3(1, 0, 0));
            hand = glm::rotate(hand, side * reach * .10f, glm::vec3(0, 0, 1));
            draw(glove, glm::scale(hand, {side * .72f, .72f, .72f}), {.66f, .605f, .51f},
                 felt, {5, 6}, .08f, 28, 0, 1, .28f);
        }
    }
    glm::vec3 cardPosition(const VisualCard& card, glm::vec3 target, bool dealer) const {
        const float a = std::clamp(card.arrival, 0.0f, 1.0f);
        const float flight = std::min(1.0f, a / .78f);
        const float eased = flight * flight * (3.0f - 2.0f * flight);
        const glm::vec3 source(dealer ? -.83f : .83f, .12f, -2.55f);
        glm::vec3 p = glm::mix(source, target, eased);
        p.y += std::sin(flight * pi) * .72f;
        if (a > .78f && a < 1) p.y += std::sin((a - .78f) / .22f * pi) * .045f;
        return p;
    }

    void drawCard(const VisualCard& card, glm::vec3 position, float rotation = 0.0f) {
        const float a = std::clamp(card.arrival, 0.0f, 1.0f);
        const float arrivalReveal = glm::smoothstep(.32f, .78f, a);
        const float reveal = card.faceDown ? 1.0f : std::clamp(card.flip, 0.0f, 1.0f) * arrivalReveal;
        glm::mat4 model = glm::translate(glm::mat4(1), position + glm::vec3(0, std::sin(reveal * pi) * .42f, 0));
        model = glm::rotate(model, rotation, glm::vec3(0, 1, 0));
        model = glm::rotate(model, (1.0f - reveal) * pi, glm::vec3(0, 0, 1));
        model = glm::scale(model, {.74f, .045f, 1.08f});
        draw(cube, model, {.90f, .86f, .72f}, 0, glm::vec2(1), .04f);
        const int rank = std::clamp(card.rank, 1, 13), suit = std::clamp(card.suit, 0, 3);
        const GLuint face = card.faceDown ? cardBack() : cardFaces[static_cast<std::size_t>(suit * 13 + rank - 1)];
        draw(plane, model, glm::vec3(1), face, glm::vec2(1), .025f);
        draw(plane, model * glm::rotate(glm::mat4(1), pi, glm::vec3(1, 0, 0)), glm::vec3(1), cardBack(),
             glm::vec2(1), .025f);
    }
    void drawHand(const std::vector<VisualCard>& cards, float rowZ, float time,
                  bool animate, bool dealer) {
        if (cards.empty()) return;
        const float count = static_cast<float>(cards.size());
        const float spacing = cards.size() < 8 ? .88f : 5.65f / std::max(count - 1.0f, 1.0f);
        for (std::size_t i = 0; i < cards.size(); ++i) {
            const float slot = static_cast<float>(i) - (count - 1.0f) * .5f;
            const float x = slot * spacing;
            const float bob = animate ? std::sin(time * 1.5f + static_cast<float>(i) * .5f) * .006f : 0.0f;
            const float angle = slot * .015f;
            const float z = rowZ + std::abs(slot) * .014f;
            // A small contact-shadow decal anchors the lifted paper to the felt.
            if (cards[i].arrival >= .78f)
                draw(plane, transform({x + .025f, -.001f, z + .025f}, {.79f, .008f, 1.13f}, angle),
                     {.018f, .080f, .048f});
            const glm::vec3 target(x, .047f + bob + static_cast<float>(i) * .002f, z);
            drawCard(cards[i], cardPosition(cards[i], target, dealer),
                     dealer ? angle : -angle);
        }
    }

    void spark(glm::vec3 p, float size, glm::vec3 color, float alpha) {
        draw(cube, transform(p, {size * .20f, size, size * .20f}), color,
             0, glm::vec2(1), 0, 8, 1.6f, alpha);
        draw(cube, transform(p, {size, size * .20f, size * .20f}), color,
             0, glm::vec2(1), 0, 8, 1.6f, alpha);
        draw(sphere, transform(p, glm::vec3(size * .17f)), {1, .91f, .61f},
             0, glm::vec2(1), 0, 8, 1.8f, alpha);
    }

    void drawEffects(const Scene& scene, bool animate) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        for (int dust = 0; dust < 32; ++dust) {
            const float phase = static_cast<float>(dust) * 2.39996f;
            const float drift = animate ? scene.time * .025f : 0;
            const float height = std::fmod(static_cast<float>(dust) * .137f + drift, 1.0f);
            const glm::vec3 p(std::sin(phase + drift * .3f) * 5.5f, -.3f + height * 3.2f,
                               -2.5f + std::cos(phase) * 3.0f);
            const float alpha = .055f * std::sin(height * pi);
            draw(sphere, transform(p, glm::vec3(.008f + .003f * static_cast<float>(dust % 3))),
                 {.81f, .58f, .28f}, 0, glm::vec2(1), 0, 8, 1.2f, alpha);
        }
        for (int hand = 0; hand < 2; ++hand) {
            const auto& cards = hand == 0 ? scene.player : scene.dealer;
            const float count = static_cast<float>(cards.size());
            const float spacing = cards.size() < 8 ? .88f : 5.65f / std::max(count - 1, 1.0f);
            for (std::size_t i = 0; i < cards.size(); ++i) {
                const auto& card = cards[i];
                const float a = std::clamp(card.arrival, 0.0f, 1.0f);
                if (a >= 1 || a <= 0) continue;
                const float slot = static_cast<float>(i) - (count - 1) * .5f;
                const glm::vec3 target(slot * spacing, .06f,
                    (hand == 0 ? 1.12f : -1.12f) + std::abs(slot) * .014f);
                const glm::vec3 tint = hand == 0 ? glm::vec3(.90f, .57f, .20f) : glm::vec3(.62f, .28f, .9f);
                for (int trail = 1; trail <= 6; ++trail) {
                    VisualCard ghost = card;
                    ghost.arrival = a - static_cast<float>(trail) * .036f;
                    if (ghost.arrival <= 0 || ghost.arrival > .78f) continue;
                    const glm::vec3 p = cardPosition(ghost, target, hand != 0);
                    draw(plane, transform(p, {.72f, .02f, 1.05f}, slot * .015f), tint,
                         cardBack(), glm::vec2(1), 0, 8, .65f,
                         .075f * (1.0f - static_cast<float>(trail) / 7));
                }
                if (a > .78f) {
                    const float landing = (a - .78f) / .22f;
                    const float opacity = std::sin(landing * pi) * .42f;
                    const float radius = .38f + landing * .64f;
                    draw(ring, transform({target.x, .014f, target.z}, {radius, .01f, radius * .7f}),
                         tint, 0, glm::vec2(1), 0, 8, 1.5f, opacity);
                    for (int particle = 0; particle < 7; ++particle) {
                        const float angle = static_cast<float>(particle) * 2 * pi / 7;
                        spark(target + glm::vec3(std::cos(angle) * radius, .10f + landing * .22f,
                                                std::sin(angle) * radius * .7f),
                              .045f * (1 - landing) + .01f, tint, opacity);
                    }
                }
            }
        }
        const float win = std::clamp(scene.celebration, 0.0f, 1.0f);
        const float rare = std::clamp(scene.legendary, 0.0f, 1.0f);
        if (win > 0 || rare > 0) {
            const float strength = std::max(win, rare);
            const float progress = 1 - strength;
            for (int i = 0; i < 38; ++i) {
                const float phase = static_cast<float>(i) * 2.39996f;
                const float radius = .45f + progress * (1.4f + static_cast<float>(i % 4) * .35f);
                const float y = .22f + progress * (1.4f + static_cast<float>(i % 5) * .22f)
                    - progress * progress * .7f;
                const glm::vec3 p(std::cos(phase) * radius, y,
                                   .55f + std::sin(phase) * radius * .57f);
                const glm::vec3 color = rare > .1f && i % 3 == 0
                    ? glm::vec3(.70f, .28f, 1.0f) : glm::vec3(1.0f, .66f, .18f);
                spark(p, .055f + static_cast<float>(i % 3) * .018f, color, strength * .78f);
            }
            draw(ring, transform({0, .018f, .55f}, {1.0f + progress * 2.1f, .01f, .60f + progress * 1.3f}),
                 {1, .66f, .18f}, 0, glm::vec2(1), 0, 8, 1.7f, strength * .25f);
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void drawChipStack(float x, float z, int height, glm::vec3 color, float phase) {
        for (int i = 0; i < height; ++i) {
            const float y = .035f + static_cast<float>(i) * .065f;
            const float offset = i > 0 ? std::sin(static_cast<float>(i) * 1.7f + phase) * .012f : 0.0f;
            draw(cylinder, transform({x + offset, y, z}, {.22f, .058f, .22f}),
                 color, chip, glm::vec2(1), .18f, 48.0f);
        }
    }

    void render(const Scene& scene, const RenderSettings& settings, int width, int height) {
        if (width <= 0 || height <= 0) return;
        activeDeck = std::clamp(scene.deckDesign, 0, 2);
        glViewport(0, 0, width, height);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        if (scene.menu) glClearColor(.026f, .014f, .050f, 1.0f);
        else glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (scene.menu) drawSky(width, height);

        int renderWidth = width;
        int renderHeight = height;
        int renderLeft = 0, renderBottom = 0;
        if (!scene.menu) {
            const auto& area = scene.tableViewport;
            const int left = std::clamp(static_cast<int>(area.left * static_cast<float>(width)), 0, width - 1);
            const int top = std::clamp(static_cast<int>(area.top * static_cast<float>(height)), 0, height - 1);
            const int right = std::clamp(static_cast<int>((area.left + area.width) * static_cast<float>(width)),
                                         left + 1, width);
            const int bottom = std::clamp(static_cast<int>((area.top + area.height) * static_cast<float>(height)),
                                          top + 1, height);
            renderWidth = right - left;
            renderHeight = bottom - top;
            renderLeft = left;
            renderBottom = height - bottom;
            glViewport(renderLeft, renderBottom, renderWidth, renderHeight);
        }

        const float yaw = glm::radians(settings.yaw);
        const float pitch = glm::radians(std::clamp(settings.pitch, 20.0f, 85.0f));
        const float distance = std::clamp(settings.distance, 5.0f, 18.0f);
        const float aspect = static_cast<float>(renderWidth) / static_cast<float>(renderHeight);
        const float menuDistance = 15.5f * std::max(1.0f, 1.45f / aspect);
        const glm::vec3 eye = scene.menu ? glm::vec3(0, 4.8f, menuDistance)
            : glm::vec3(distance * std::cos(pitch) * std::sin(yaw),
                        distance * std::sin(pitch),
                        distance * std::cos(pitch) * std::cos(yaw));
        const glm::mat4 view = glm::lookAt(eye, scene.menu ? glm::vec3(0, 1.55f, 0)
                                                                         : glm::vec3(0, .60f, -.50f),
                                         glm::vec3(0, 1, 0));
        const glm::mat4 projection = glm::perspective(glm::radians(43.0f),
            aspect, .1f, 60.0f);
        glUseProgram(program.id);
        glActiveTexture(GL_TEXTURE0);
        glUniformMatrix4fv(u.view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(u.projection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(u.camera, 1, glm::value_ptr(eye));
        const float lightMotion = settings.animate ? std::sin(scene.time * .17f) * .85f : 0;
        glUniform3f(u.light, scene.menu ? -5.0f : -2.2f + lightMotion, 7.0f, 4.0f);
        if (scene.menu) glUniform3f(u.fogColor, .25f, .17f, .22f);
        else glUniform3f(u.fogColor, 0, 0, 0);
        glUniform1f(u.intensity, std::clamp(settings.lightIntensity, 0.0f, 3.0f));
        glUniform1i(u.lighting, settings.lighting ? GL_TRUE : GL_FALSE);
        glUniform1f(u.roomLight, scene.menu ? 0.0f : 1.0f);
        glUniform1f(u.lampBrightness, roomLampBrightness(scene.time, settings.animate));
        glUniform4f(u.viewport, static_cast<float>(renderLeft), static_cast<float>(renderBottom),
                    static_cast<float>(renderWidth), static_cast<float>(renderHeight));
        texturesEnabled = settings.textures;

        if (scene.menu) {
            drawCasino(scene.time, settings.animate);
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
            return;
        }

        drawRoom(scene.time, settings.animate);
        drawDealer(scene, settings.animate);

        // Layered elliptical cylinders form the table and its narrow brass rim.
        draw(cylinder, transform({0, -.16f, 0}, {4.2f, .18f, 3.0f}),
             {.10f, .038f, .055f}, 0, glm::vec2(1), .22f, 48.0f);
        draw(cylinder, transform({0, -.085f, 0}, {4.05f, .06f, 2.85f}),
             {.78f, .52f, .18f}, 0, glm::vec2(1), .5f, 72.0f);
        draw(cylinder, transform({0, -.045f, 0}, {3.95f, .04f, 2.75f}),
             {.028f, .060f, .043f}, 0, glm::vec2(1), .1f);
        draw(cylinder, transform({0, -.025f, 0}, {3.83f, .04f, 2.62f}),
             {.034f, .28f, .155f}, felt, {14, 11}, .025f, 10.0f, 0, 1, 1);

        // Quiet gold geometry marks the middle of the playing field.
        const glm::vec3 mutedGold{.54f, .35f, .18f};
        draw(cube, transform({-1.55f, .002f, 0}, {2.2f, .007f, .012f}), mutedGold);
        draw(cube, transform({1.55f, .002f, 0}, {2.2f, .007f, .012f}), mutedGold);
        draw(cube, transform({0, .002f, 0}, {.16f, .008f, .16f}, pi * .25f), mutedGold);
        for (const float side : {-1.0f, 1.0f}) {
            draw(cube, transform({side * 2.84f, .002f, side * 1.32f}, {.22f, .008f, .22f}, pi * .25f),
                 {.44f, .14f, .24f});
            draw(cube, transform({side * 2.63f, .002f, side * 1.53f}, {.16f, .008f, .16f}, pi * .25f),
                 mutedGold);
        }

        // Deck: offset layers make thickness and the transformation pipeline visible.
        for (int i = 0; i < 12; ++i) {
            draw(cube, transform({2.98f, .026f + static_cast<float>(i) * .022f, -.04f},
                                 {.74f, .019f, 1.08f}, -.08f),
                 i % 2 == 0 ? glm::vec3(.89f, .85f, .71f) : glm::vec3(.69f, .66f, .55f));
        }
        draw(plane, transform({2.98f, .272f, -.04f}, {.74f, .019f, 1.08f}, -.08f),
             glm::vec3(1), cardBack());

        const int available = std::clamp(scene.coins, 0, 1000);
        drawChipStack(-3.0f, -.12f, 2 + std::min(available / 10, 5), {.66f, .17f, .14f}, .3f);
        drawChipStack(-2.56f, -.40f, 2 + std::min(available / 20, 4), {.37f, .15f, .58f}, 1.0f);
        drawChipStack(-2.53f, .24f, 1 + std::min(available / 30, 3), {.67f, .49f, .19f}, 2.0f);

        drawHand(scene.dealer, -1.12f, scene.time, settings.animate, true);
        drawHand(scene.player, 1.12f, scene.time + .7f, settings.animate, false);
        drawEffects(scene, settings.animate);

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        // The overlay renderer can now set its own blend/depth state.
    }
};

Renderer::Renderer(const std::filesystem::path& assetRoot)
    : impl_(std::make_unique<Impl>(assetRoot)) {}
Renderer::~Renderer() = default;

void Renderer::render(const Scene& scene, const RenderSettings& settings,
                      int framebufferWidth, int framebufferHeight) {
    impl_->render(scene, settings, framebufferWidth, framebufferHeight);
}

} // namespace gfx
