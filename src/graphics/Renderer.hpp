#pragma once

#include <filesystem>
#include <memory>
#include <vector>

namespace gfx {

struct VisualCard {
    int rank = 1; // Ace = 1, Jack = 11, Queen = 12, King = 13.
    int suit = 0; // Clubs, diamonds, hearts, spades (same order as bj::Suit).
    bool faceDown = false;
};

struct Scene {
    std::vector<VisualCard> player;
    std::vector<VisualCard> dealer;
    int coins = 0;
    float time = 0.0f;
    bool menu = false;
};

struct RenderSettings {
    float yaw = 0.0f;
    float pitch = 53.0f;
    float distance = 10.0f;
    float lightIntensity = 1.2f;
    bool textures = true;
    bool lighting = true;
    bool animate = true;
};

// Construct and destroy while the GLFW OpenGL context is current.
class Renderer {
public:
    explicit Renderer(const std::filesystem::path& assetRoot);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void render(const Scene& scene, const RenderSettings& settings,
                int framebufferWidth, int framebufferHeight);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gfx
