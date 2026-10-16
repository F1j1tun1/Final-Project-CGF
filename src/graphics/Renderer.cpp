#include "Renderer.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
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

Mesh makeCone(int segments) {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    for (int i = 0; i < segments; ++i) {
        const float a = static_cast<float>(i) * 2.0f * pi / static_cast<float>(segments);
        const float b = static_cast<float>(i + 1) * 2.0f * pi / static_cast<float>(segments);
        const glm::vec3 normal = glm::normalize(glm::vec3(std::cos((a + b) * .5f), 1.0f,
                                                        std::sin((a + b) * .5f)));
        const auto offset = static_cast<std::uint32_t>(vertices.size() / 8);
        const float u = static_cast<float>(i) / static_cast<float>(segments);
        const float nextU = static_cast<float>(i + 1) / static_cast<float>(segments);
        vertices.insert(vertices.end(), {
            std::cos(a), -.5f, std::sin(a), normal.x, normal.y, normal.z, u, 0,
            0, .5f, 0, normal.x, normal.y, normal.z, (u + nextU) * .5f, 1,
            std::cos(b), -.5f, std::sin(b), normal.x, normal.y, normal.z, nextU, 0});
        indices.insert(indices.end(), {offset, offset + 1, offset + 2});
    }
    return makeMesh(vertices, indices);
}

Mesh makeSphere(int slices, int rings) {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    for (int ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float latitude = v * pi;
        for (int slice = 0; slice <= slices; ++slice) {
            const float u = static_cast<float>(slice) / static_cast<float>(slices);
            const float longitude = u * 2.0f * pi;
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
    drawText(bitmap, value, 13, 14, 3, ink);
    drawSuit(bitmap, suit, 22, 51, 10, ink);
    drawText(bitmap, value, 13, 14, 3, ink, true);
    drawSuit(bitmap, suit, 22, 51, 10, ink, true);

    if (rank > 10 || rank == 1) {
        drawSuit(bitmap, suit, 80, 111, rank == 1 ? 45.0f : 34.0f, ink);
        if (rank > 10) {
            drawText(bitmap, value, 66, 62, 5, ink);
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

Bitmap roofBitmap() {
    Bitmap bitmap(96, 64, {93, 36, 119});
    for (int x = 0; x < bitmap.width; ++x) {
        const int stripe = x / 8;
        const Pixel color = stripe % 3 == 0 ? Pixel{93, 36, 119}
            : stripe % 3 == 1 ? Pixel{177, 49, 67} : Pixel{218, 164, 61};
        for (int y = 0; y < bitmap.height; ++y) {
            bitmap.set(x, y, color);
            if (x % 8 == 0) bitmap.set(x, y, {242, 199, 98});
        }
    }
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
    Mesh cube;
    Mesh plane;
    Mesh cylinder;
    Mesh cone;
    Mesh sphere;
    Textures textures;
    std::array<GLuint, 52> cardFaces{};
    GLuint white = 0;
    GLuint felt = 0;
    GLuint back = 0;
    GLuint chip = 0;
    GLuint roof = 0;
    struct Locations {
        GLint model, view, projection, normal, color, uvScale, camera, light,
              intensity, shininess, specular, emission, useTexture, lighting;
    } u{};
    bool texturesEnabled = true;

    explicit Impl(const std::filesystem::path& root) {
        program.load(root / "shaders" / "table.vert", root / "shaders" / "table.frag");
        cube = makeCube();
        plane = makeTopPlane();
        cylinder = makeCylinder(80);
        cone = makeCone(48);
        sphere = makeSphere(24, 16);
        white = textures.upload(Bitmap(1, 1, {255, 255, 255}));
        felt = textures.upload(feltBitmap(), true);
        back = textures.upload(cardBackBitmap());
        chip = textures.upload(chipBitmap());
        roof = textures.upload(roofBitmap(), true);
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
        u.useTexture = uniform("uUseTexture");
        u.lighting = uniform("uLighting");
        glUseProgram(program.id);
        glUniform1i(uniform("uTexture"), 0);
        glUseProgram(0);
    }

    void draw(const Mesh& mesh, const glm::mat4& model, glm::vec3 color,
              GLuint texture = 0, glm::vec2 uvScale = glm::vec2(1.0f),
              float specular = .08f, float shininess = 32.0f, float emission = 0.0f) {
        const glm::mat3 normal = glm::transpose(glm::inverse(glm::mat3(model)));
        glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix3fv(u.normal, 1, GL_FALSE, glm::value_ptr(normal));
        glUniform3fv(u.color, 1, glm::value_ptr(color));
        glUniform2fv(u.uvScale, 1, glm::value_ptr(uvScale));
        glUniform1f(u.shininess, shininess);
        glUniform1f(u.specular, specular);
        glUniform1f(u.emission, emission);
        glUniform1i(u.useTexture, texturesEnabled && texture != 0 ? GL_TRUE : GL_FALSE);
        glBindTexture(GL_TEXTURE_2D, texture ? texture : white);
        mesh.draw();
    }

    void beam(glm::vec3 from, glm::vec3 to, float radius, glm::vec3 color) {
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
        draw(cylinder, glm::scale(model, {radius, length, radius}), color,
             0, glm::vec2(1), .25f);
    }

    void diamond(glm::vec3 position, float size, glm::vec3 color) {
        glm::mat4 model = glm::translate(glm::mat4(1), position);
        model = glm::rotate(model, pi * .25f, glm::vec3(0, 0, 1));
        draw(cube, glm::scale(model, {size, size, .025f}), color);
    }

    void drawCasino(float time, bool animate) {
        const glm::vec3 plum{.20f, .072f, .28f};
        const glm::vec3 deepPurple{.105f, .038f, .16f};
        const glm::vec3 gold{.72f, .46f, .16f};
        const glm::vec3 crimson{.42f, .060f, .105f};
        const glm::vec3 cream{.84f, .71f, .43f};
        const glm::vec3 window{1.0f, .58f, .18f};
        constexpr float center = 2.8f;
        constexpr float front = -.85f;

        // A quiet courtyard leaves space for the menu and frames the warm facade.
        draw(cube, transform({0, -.22f, 0}, {35, .35f, 25}), {.055f, .035f, .078f});
        for (int row = 0; row < 6; ++row) {
            const float z = .2f + static_cast<float>(row) * .95f;
            for (int column = 0; column < 4; ++column) {
                const float x = center + (static_cast<float>(column) - 1.5f) * .68f;
                draw(cube, transform({x, -.032f, z}, {.62f, .025f, .86f}),
                     (row + column) % 2 == 0 ? glm::vec3(.12f, .078f, .145f)
                                              : glm::vec3(.085f, .055f, .11f));
            }
        }
        draw(cube, transform({center, .04f, -.15f}, {6.8f, .17f, 3.6f}), {.13f, .085f, .16f});
        draw(cube, transform({center, .17f, -.50f}, {6.25f, .16f, 2.7f}), gold);
        draw(cube, transform({center, 1.89f, -1.85f}, {5.45f, 3.4f, 2.0f}), plum);
        draw(cube, transform({center, .55f, front + .025f}, {5.55f, .66f, .10f}), crimson);
        for (const float y : {.25f, 1.15f, 3.50f})
            draw(cube, transform({center, y, front + .065f}, {5.68f, .08f, .14f}), gold,
                 0, glm::vec2(1), .45f, 64);

        // Two turrets and striped tent roofs give the casino a jester silhouette.
        for (const float x : {center - 2.78f, center + 2.78f}) {
            draw(cylinder, transform({x, 2.15f, -1.77f}, {.69f, 4.0f, .69f}), deepPurple);
            for (const float y : {.30f, 1.17f, 3.91f})
                draw(cylinder, transform({x, y, -1.77f}, {.74f, .10f, .74f}), gold,
                     0, glm::vec2(1), .45f, 64);
            draw(cone, transform({x, 4.74f, -1.77f}, {.98f, 1.60f, .98f}),
                 glm::vec3(1), roof, glm::vec2(1), .12f);
            draw(sphere, transform({x, 5.56f, -1.77f}, {.12f, .12f, .12f}), gold,
                 0, glm::vec2(1), .45f, 64);
            draw(cube, transform({x, 2.45f, -1.075f}, {.34f, .91f, .025f}), gold);
            draw(cube, transform({x, 2.45f, -1.055f}, {.24f, .79f, .025f}),
                 window, 0, glm::vec2(1), .0f, 8, .65f);
            draw(cube, transform({x, 2.45f, -1.03f}, {.035f, .82f, .035f}), deepPurple);
            draw(cube, transform({x, 2.45f, -1.03f}, {.27f, .035f, .035f}), deepPurple);
        }
        draw(cone, transform({center, 4.13f, -1.84f}, {3.02f, 1.30f, 1.36f}),
             glm::vec3(1), roof, glm::vec2(1), .12f);
        draw(sphere, transform({center, 4.84f, -1.84f}, {.14f, .14f, .14f}), gold,
             0, glm::vec2(1), .45f, 64);

        // The central entrance is an arched recess with twin red doors.
        draw(cube, transform({center, .90f, front + .03f}, {1.26f, 1.45f, .13f}), gold);
        draw(sphere, transform({center, 1.59f, front + .03f}, {.63f, .60f, .10f}), gold);
        draw(cube, transform({center, .90f, front + .11f}, {1.10f, 1.40f, .08f}), {.045f, .018f, .066f});
        draw(sphere, transform({center, 1.59f, front + .12f}, {.54f, .51f, .065f}), {.045f, .018f, .066f});
        for (const float side : {-1.0f, 1.0f}) {
            const float x = center + side * .267f;
            draw(cube, transform({x, .84f, front + .17f}, {.49f, 1.20f, .06f}), crimson);
            draw(cube, transform({x, .84f, front + .205f}, {.38f, 1.05f, .025f}), {.58f, .12f, .17f});
            draw(sphere, transform({center + side * .095f, .86f, front + .25f}, {.035f, .035f, .035f}), gold);
        }
        draw(cube, transform({center, .23f, front + .63f}, {1.8f, .13f, .84f}), cream);
        draw(cube, transform({center, .105f, front + .97f}, {2.10f, .13f, .70f}), {.54f, .43f, .31f});

        // Sparse windows, pennants and the three-bell crest keep the facade readable.
        for (const float x : {center - 1.78f, center + 1.78f}) {
            draw(cube, transform({x, 2.39f, front + .02f}, {.66f, .92f, .10f}), gold);
            draw(cube, transform({x, 2.39f, front + .085f}, {.52f, .78f, .05f}),
                 window, 0, glm::vec2(1), .0f, 8, .65f);
            draw(cube, transform({x, 2.39f, front + .12f}, {.045f, .84f, .04f}), deepPurple);
            draw(cube, transform({x, 2.39f, front + .12f}, {.59f, .045f, .04f}), deepPurple);
            draw(cube, transform({x, 1.47f, front + .06f}, {.58f, .55f, .04f}), crimson);
            diamond({x, 1.48f, front + .095f}, .23f, cream);
        }
        draw(cube, transform({center, 2.91f, front + .08f}, {1.50f, .75f, .10f}), gold);
        draw(cube, transform({center, 2.91f, front + .145f}, {1.38f, .63f, .05f}), deepPurple);
        diamond({center, 2.83f, front + .19f}, .28f, crimson);
        for (int i = -1; i <= 1; ++i) {
            const float x = center + static_cast<float>(i) * .34f;
            beam({center, 2.86f, front + .21f}, {x, 3.10f, front + .21f}, .027f, cream);
            draw(sphere, transform({x, 3.12f, front + .21f}, {.07f, .07f, .045f}), cream,
                 0, glm::vec2(1), .2f, 32, .12f);
        }
        for (const float x : {center - 3.65f, center + 3.65f}) {
            draw(cylinder, transform({x, .88f, 1.0f}, {.055f, 1.8f, .055f}), gold);
            draw(cylinder, transform({x, .04f, 1.0f}, {.21f, .10f, .21f}), deepPurple);
            const float pulse = animate ? .04f * std::sin(time * 1.6f + x) : 0.0f;
            draw(sphere, transform({x, 1.90f, 1.0f}, {.19f, .27f, .19f}),
                 window, 0, glm::vec2(1), .0f, 8, .70f + pulse);
            draw(cone, transform({x, 2.17f, 1.0f}, {.28f, .20f, .28f}), deepPurple);
        }
    }

    void drawRoom() {
        const glm::vec3 purple{.11f, .033f, .17f};
        const glm::vec3 gold{.55f, .36f, .14f};
        draw(cube, transform({0, -.90f, 0}, {28, .25f, 24}), {.055f, .029f, .073f});
        draw(cube, transform({0, 1.80f, -5.35f}, {17, 6, .22f}), {.065f, .030f, .10f});
        for (const float side : {-1.0f, 1.0f}) {
            for (int i = 0; i < 5; ++i) {
                const float x = side * (4.1f + static_cast<float>(i) * .49f);
                draw(cylinder, transform({x, 1.80f, -5.06f}, {.34f, 5.50f, .22f}),
                     i % 2 == 0 ? purple : glm::vec3(.20f, .037f, .087f));
            }
            draw(cylinder, transform({side * 3.70f, 1.64f, -4.85f}, {.17f, 5.1f, .17f}), gold);
            for (int row = 0; row < 3; ++row)
                diamond({side * 2.15f, .65f + static_cast<float>(row) * 1.10f, -5.20f}, .28f,
                        row % 2 == 0 ? gold * .65f : glm::vec3(.27f, .06f, .15f));
        }
        draw(cube, transform({0, 3.95f, -5.02f}, {15, .10f, .14f}), gold);
    }

    void drawJester(float time, bool animate) {
        const glm::vec3 purple{.34f, .10f, .45f};
        const glm::vec3 red{.65f, .10f, .18f};
        const glm::vec3 gold{.85f, .62f, .23f};
        const glm::vec3 mask{.91f, .83f, .66f};
        const float sway = animate ? std::sin(time * .65f) * .028f : 0.0f;
        constexpr float z = -3.35f;
        // Keep the full cap below the HUD, with the hands resting just inside the rail.
        constexpr float figureScale = .65f;
        const glm::vec3 pivot{0, .16f, -2.74f};
        const glm::vec3 shift{0, 0, .45f};
        glm::mat4 group = glm::translate(glm::mat4(1), pivot + shift);
        group = glm::scale(group, glm::vec3(figureScale));
        group = glm::translate(group, -pivot);
        const auto place = [&](glm::vec3 point) { return pivot + shift + (point - pivot) * figureScale; };
        draw(sphere, group * transform({sway, .62f, z}, {.48f, .67f, .30f}), purple);
        draw(sphere, group * transform({.20f + sway, .62f, z + .025f}, {.25f, .59f, .30f}), red);
        for (const float side : {-1.0f, 1.0f}) {
            const glm::vec3 sleeve = side < 0 ? purple : red;
            beam(place({side * .34f + sway, .98f, z}), place({side * .64f, .24f, -2.77f}), .13f * figureScale, sleeve);
            draw(sphere, group * transform({side * .64f, .16f, -2.74f}, {.14f, .10f, .18f}), mask);
            diamond(place({side * .22f + sway, 1.24f, z + .29f}), .22f * figureScale, gold);
        }
        draw(sphere, group * transform({sway, 1.69f, z}, {.30f, .37f, .26f}), mask,
             0, glm::vec2(1), .06f);
        for (const float side : {-1.0f, 1.0f}) {
            draw(sphere, group * transform({side * .105f + sway, 1.77f, z + .244f}, {.064f, .027f, .025f}), {.045f, .018f, .065f});
            diamond(place({side * .17f + sway, 1.64f, z + .225f}), .058f * figureScale, red);
        }
        draw(sphere, group * transform({sway, 1.68f, z + .27f}, {.040f, .054f, .055f}), gold);
        beam(place({-.10f + sway, 1.57f, z + .244f}), place({sway, 1.54f, z + .26f}), .012f * figureScale, red);
        beam(place({sway, 1.54f, z + .26f}), place({.10f + sway, 1.57f, z + .244f}), .012f * figureScale, red);
        draw(sphere, group * transform({sway, 2.005f, z}, {.31f, .13f, .26f}), purple);
        for (int i = -1; i <= 1; ++i) {
            const float side = static_cast<float>(i);
            const float tilt = -.57f * side;
            glm::mat4 model = glm::translate(glm::mat4(1), {side * .23f + sway, 2.22f, z});
            model = glm::rotate(model, tilt, glm::vec3(0, 0, 1));
            draw(cone, group * glm::scale(model, {.16f, i == 0 ? .65f : .58f, .16f}), i == 0 ? purple : red);
            draw(sphere, group * transform({side * .39f + sway, i == 0 ? 2.57f : 2.47f, z}, {.066f, .066f, .066f}), gold,
                 0, glm::vec2(1), .45f, 64);
        }
        for (int i = 0; i < 3; ++i)
            draw(sphere, group * transform({sway, .90f - static_cast<float>(i) * .25f, z + .31f}, {.045f, .045f, .028f}), gold);
    }

    void drawCard(const VisualCard& card, glm::vec3 position, float rotation = 0.0f) {
        // A solid cuboid gives visible paper edges; its upper face receives the card texture.
        const glm::mat4 model = transform(position, {.74f, .045f, 1.08f}, rotation);
        draw(cube, model, {.90f, .86f, .72f}, 0, glm::vec2(1), .04f);
        const int rank = std::clamp(card.rank, 1, 13);
        const int suit = std::clamp(card.suit, 0, 3);
        const GLuint face = card.faceDown ? back
            : cardFaces[static_cast<std::size_t>(suit * 13 + rank - 1)];
        draw(plane, model, glm::vec3(1.0f), face, glm::vec2(1), .025f);
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
            draw(plane, transform({x + .025f, -.001f, z + .025f}, {.79f, .008f, 1.13f}, angle),
                 {.055f, .014f, .085f});
            drawCard(cards[i], {x, .047f + bob + static_cast<float>(i) * .002f, z},
                     dealer ? angle : -angle);
        }
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
        glViewport(0, 0, width, height);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(.026f, .014f, .050f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const float yaw = glm::radians(settings.yaw);
        const float pitch = glm::radians(std::clamp(settings.pitch, 20.0f, 85.0f));
        const float distance = std::clamp(settings.distance, 5.0f, 18.0f);
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        const float menuDistance = 16.0f * std::max(1.0f, 1.45f / aspect);
        const glm::vec3 eye = scene.menu ? glm::vec3(0, 4.2f, menuDistance)
            : glm::vec3(distance * std::cos(pitch) * std::sin(yaw),
                        distance * std::sin(pitch),
                        distance * std::cos(pitch) * std::cos(yaw));
        const glm::mat4 view = glm::lookAt(eye, scene.menu ? glm::vec3(0, 1.8f, 0)
                                                                         : glm::vec3(0, 0, 0),
                                         glm::vec3(0, 1, 0));
        const glm::mat4 projection = glm::perspective(glm::radians(43.0f),
            aspect, .1f, 60.0f);
        glUseProgram(program.id);
        glActiveTexture(GL_TEXTURE0);
        glUniformMatrix4fv(u.view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(u.projection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(u.camera, 1, glm::value_ptr(eye));
        glUniform3f(u.light, scene.menu ? 1.0f : -3.0f, 7.0f, 3.0f);
        glUniform1f(u.intensity, std::clamp(settings.lightIntensity, 0.0f, 3.0f));
        glUniform1i(u.lighting, settings.lighting ? GL_TRUE : GL_FALSE);
        texturesEnabled = settings.textures;

        if (scene.menu) {
            drawCasino(scene.time, settings.animate);
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
            return;
        }

        drawRoom();
        drawJester(scene.time, settings.animate);

        // Layered elliptical cylinders form the table and its narrow brass rim.
        draw(cylinder, transform({0, -.35f, 0}, {4.2f, .5f, 3.0f}),
             {.15f, .048f, .13f}, 0, glm::vec2(1), .22f, 48.0f);
        draw(cylinder, transform({0, -.085f, 0}, {4.05f, .06f, 2.85f}),
             {.78f, .52f, .18f}, 0, glm::vec2(1), .5f, 72.0f);
        draw(cylinder, transform({0, -.045f, 0}, {3.95f, .04f, 2.75f}),
             {.105f, .025f, .15f}, 0, glm::vec2(1), .1f);
        draw(cylinder, transform({0, -.025f, 0}, {3.83f, .04f, 2.62f}),
             {.26f, .075f, .35f}, felt, {14, 11}, .015f, 8.0f);

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
             glm::vec3(1), back);

        const int available = std::clamp(scene.coins, 0, 1000);
        drawChipStack(-3.0f, -.12f, 2 + std::min(available / 10, 5), {.66f, .17f, .14f}, .3f);
        drawChipStack(-2.56f, -.40f, 2 + std::min(available / 20, 4), {.37f, .15f, .58f}, 1.0f);
        drawChipStack(-2.53f, .24f, 1 + std::min(available / 30, 3), {.67f, .49f, .19f}, 2.0f);

        drawHand(scene.dealer, -1.12f, scene.time, settings.animate, true);
        drawHand(scene.player, 1.12f, scene.time + .7f, settings.animate, false);

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
