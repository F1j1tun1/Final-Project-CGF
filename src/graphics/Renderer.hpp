#pragma once

#include <filesystem>
#include <memory>
#include <vector>

namespace gfx {

struct VisualCard {
    int rank = 1; // Ace = 1, Jack = 11, Queen = 12, King = 13.
    int suit = 0; // Clubs, diamonds, hearts, spades (same order as bj::Suit).
    bool faceDown = false;
    float arrival = 1.0f; // 0 = leaving the dealer, 1 = settled in this hand.
    float flip = 1.0f; // 0 = back, 1 = the side selected by faceDown.
};

// Normalized framebuffer rectangle, measured from the top-left like the UI.
struct Viewport {
    float left = 0.0f;
    float top = 0.0f;
    float width = 1.0f;
    float height = 1.0f;
};

struct Scene {
    std::vector<VisualCard> player;
    std::vector<VisualCard> dealer;
    int coins = 0;
    float time = 0.0f;
    bool menu = false;
    Viewport tableViewport;
    float dealerReach = 0.0f;
    bool dealingToPlayer = true;
    float celebration = 0.0f; // Presentation envelope: 1 at the win, decays to 0.
    float legendary = 0.0f; // Presentation envelope for rare gold/violet sparks.
    int deckDesign = 0; // Hands, Life, Coins.
    float bustReaction = 0.0f; // Presentation envelope: 1 at a player bust, decays to 0.
};

inline constexpr float minLight = .55f, maxLight = 2.5f;
inline constexpr float minZoom = 3.5f, maxZoom = 14.0f;

struct RenderSettings {
    float yaw = 0.0f;
    float pitch = 32.0f;
    float distance = 9.1f;
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
