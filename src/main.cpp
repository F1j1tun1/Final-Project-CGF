#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "game/Game.hpp"
#include "graphics/Renderer.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr ImVec4 gold{0.95f, 0.76f, 0.39f, 1.0f};
constexpr ImVec4 muted{0.57f, 0.67f, 0.64f, 1.0f};
constexpr ImVec4 green{0.45f, 0.91f, 0.70f, 1.0f};
constexpr ImVec4 red{1.0f, 0.43f, 0.43f, 1.0f};
constexpr ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoBringToFrontOnFocus;

struct Options {
    std::uint32_t seed = 42;
    int frames = 0;
    bool hidden = false;
    bool demo = false;
    bool autoplay = false;
    std::filesystem::path screenshot;
    std::filesystem::path assets;
};

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&]() -> std::string {
            if (++i >= argc) throw std::runtime_error("Missing value after " + arg);
            return argv[i];
        };
        if (arg == "--seed") {
            const auto seed = std::stoull(value());
            if (seed > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("Seed must fit in uint32.");
            options.seed = static_cast<std::uint32_t>(seed);
        } else if (arg == "--frames") {
            options.frames = std::stoi(value());
            if (options.frames < 1) throw std::runtime_error("Frames must be positive.");
        } else if (arg == "--screenshot") options.screenshot = value();
        else if (arg == "--assets") options.assets = value();
        else if (arg == "--hidden") options.hidden = true;
        else if (arg == "--demo") options.demo = true;
        else if (arg == "--autoplay") options.autoplay = true;
        else if (arg == "--smoke") {
            options.hidden = true;
            options.autoplay = true;
            options.frames = 180;
        } else if (arg == "--help") {
            std::cout << "Blackjack Forge\n"
                "  --seed N          Reproducible run\n"
                "  --demo            Deal the first hand on launch\n"
                "  --frames N        Exit after N rendered frames\n"
                "  --screenshot FILE Save the final frame as PNG\n"
                "  --assets DIR      Override shader directory\n"
                "  --hidden          Create an invisible graphics window\n"
                "  --autoplay        Exercise the game state machine\n"
                "  --smoke           Hidden 180-frame integration run\n";
            std::exit(0);
        } else throw std::runtime_error("Unknown option: " + arg);
    }
    if (!options.screenshot.empty() && options.frames == 0) options.frames = 3;
    return options;
}

struct Window {
    GLFWwindow* handle = nullptr;
    explicit Window(bool hidden) {
        glfwSetErrorCallback([](int code, const char* description) {
            std::cerr << "GLFW " << code << ": " << description << '\n';
        });
        if (!glfwInit()) throw std::runtime_error("GLFW initialization failed.");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
        glfwWindowHint(GLFW_SAMPLES, 4);
        glfwWindowHint(GLFW_VISIBLE, hidden ? GLFW_FALSE : GLFW_TRUE);
        handle = glfwCreateWindow(1440, 900, "Blackjack Forge", nullptr, nullptr);
        if (!handle) {
            glfwTerminate();
            throw std::runtime_error("OpenGL 3.3 window creation failed. Check the GPU driver.");
        }
        glfwMakeContextCurrent(handle);
        if (!gladLoadGL(glfwGetProcAddress)) {
            glfwDestroyWindow(handle);
            glfwTerminate();
            throw std::runtime_error("Could not load OpenGL functions.");
        }
        glfwSetWindowSizeLimits(handle, 1100, 720, GLFW_DONT_CARE, GLFW_DONT_CARE);
        glfwSwapInterval(hidden ? 0 : 1);
        std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n'
                  << "Renderer: " << glGetString(GL_RENDERER) << '\n';
    }
    ~Window() {
        if (handle) glfwDestroyWindow(handle);
        glfwTerminate();
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

struct Interface {
    bool glfwBackend = false;
    bool glBackend = false;
    explicit Interface(GLFWwindow* window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImFontConfig font;
        font.SizePixels = 17.0f;
        io.Fonts->AddFontDefault(&font);
        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        style.WindowRounding = 14.0f;
        style.FrameRounding = 8.0f;
        style.WindowBorderSize = 1.0f;
        style.FramePadding = ImVec2(15, 11);
        style.WindowPadding = ImVec2(20, 20);
        style.ItemSpacing = ImVec2(10, 12);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.075f, 0.068f, 0.95f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.22f, 0.35f, 0.29f, 0.7f);
        style.Colors[ImGuiCol_Text] = ImVec4(0.91f, 0.94f, 0.89f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.12f, 0.29f, 0.22f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.43f, 0.31f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.29f, 0.55f, 0.38f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogram] = gold;
        glfwBackend = ImGui_ImplGlfw_InitForOpenGL(window, true);
        if (glfwBackend) glBackend = ImGui_ImplOpenGL3_Init("#version 330 core");
        if (!glfwBackend || !glBackend) {
            if (glfwBackend) ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            throw std::runtime_error("ImGui backend initialization failed.");
        }
    }
    ~Interface() {
        if (glBackend) ImGui_ImplOpenGL3_Shutdown();
        if (glfwBackend) ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
};

void beginPanel(const char* name, ImVec2 position, ImVec2 size, bool transparent = false, bool overlay = false) {
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    const auto flags = overlay ? panelFlags & ~ImGuiWindowFlags_NoBringToFrontOnFocus : panelFlags;
    ImGui::Begin(name, nullptr, flags | (transparent ? ImGuiWindowFlags_NoBackground : 0));
}

void heading(const char* text) {
    ImGui::TextColored(gold, "%s", text);
    ImGui::Separator();
    ImGui::Spacing();
}

void primaryAction(bj::Game& game) {
    switch (game.state().phase) {
        case bj::Phase::Ready: game.deal(); break;
        case bj::Phase::RoundOver: game.advance(); break;
        case bj::Phase::Shop: game.leaveShop(); break;
        case bj::Phase::RunWon:
        case bj::Phase::RunLost: game.restart(game.state().seed + 1); break;
        default: break;
    }
}

void drawInterface(bj::Game& game, gfx::RenderSettings& settings, bool& debug) {
    const bj::Snapshot state = game.state();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float w = display.x;
    const float h = display.y;
    const float sideWidth = w < 1250 ? 205.0f : 235.0f;

    beginPanel("##header", {24, 10}, {w - 48, 90}, true);
    ImGui::SetWindowFontScale(1.65f);
    ImGui::TextUnformatted("BLACKJACK  /  FORGE");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(muted, "Build your luck. Beat the house.");
    ImGui::SetCursorPos({w - 365, 28});
    if (ImGui::Button(debug ? "Close settings [F1]" : "Settings [F1]")) debug = !debug;
    ImGui::End();

    beginPanel("##run", {24, 120}, {sideWidth, 355});
    heading("THE RUN");
    ImGui::Text("Table %d / 3", state.stage);
    ImGui::PushStyleColor(ImGuiCol_Text, muted);
    ImGui::TextWrapped("%s", state.stage == 3 ? "Boss: dealer draws to 18" : "Dealer stands on 17");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::SetWindowFontScale(1.65f);
    ImGui::Text("%d / %d", state.stageScore, state.goal);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(muted, "STAGE POINTS");
    ImGui::ProgressBar(std::min(1.0f, static_cast<float>(state.stageScore) / state.goal), {-1, 9}, "");
    ImGui::Spacing();
    ImGui::Text("Hands left     %d", state.roundsLeft);
    ImGui::TextColored(gold, "Coins          %d", state.coins);
    ImGui::Text("Win streak     %d", state.streak);
    ImGui::TextColored(muted, "Seed %u", state.seed);
    ImGui::End();

    beginPanel("##relics", {w - sideWidth - 24, 120}, {sideWidth, h - 240});
    heading("YOUR RELICS");
    if (state.owned.empty()) {
        ImGui::TextWrapped("Win a table to earn coins and visit the shop.");
    } else {
        for (const auto& item : bj::relicCatalog()) {
            if (!game.hasRelic(item.id)) continue;
            ImGui::TextColored(gold, "%s", item.name);
            ImGui::TextWrapped("%s", item.description);
            if (item.id == bj::Relic::SecondChance)
                ImGui::TextColored(muted, "%s", state.usedSecondChance ? "Used on this table" : "Ready on this table");
            ImGui::Spacing();
        }
    }
    ImGui::End();

    beginPanel("##guide", {24, 495}, {sideWidth, h - 615});
    heading("HOW TO PLAY");
    ImGui::TextWrapped("Get closer to 21 than the dealer. Going over 21 loses the hand.");
    ImGui::TextWrapped("Only wins earn points. Reach the target before your hands run out.");
    ImGui::TextColored(muted, "H: hit   S: stand");
    ImGui::TextColored(muted, "Space: next action");
    ImGui::End();

    const float centerWidth = w - 2 * sideWidth - 100;
    beginPanel("##dealer", {sideWidth + 50, 115}, {centerWidth, 80}, true);
    const bool hiddenCard = state.phase == bj::Phase::PlayerTurn;
    if (state.dealer.empty()) ImGui::TextColored(muted, "DEALER  /  Waiting for the deal");
    else if (hiddenCard) {
        const int showing = bj::evaluateHand({state.dealer.front()}).total;
        ImGui::Text("DEALER  /  %d + hidden card", showing);
    } else ImGui::Text("DEALER  /  %d", bj::evaluateHand(state.dealer).total);
    ImGui::End();

    beginPanel("##actions", {sideWidth + 50, h - 225}, {centerWidth, 205});
    const auto hand = bj::evaluateHand(state.player);
    ImGui::TextColored(muted, "YOUR HAND");
    ImGui::SameLine();
    if (state.player.empty()) ImGui::TextUnformatted("--");
    else ImGui::TextColored(hand.bust ? red : green, "%d%s", hand.total, hand.soft ? "  (soft)" : "");
    if (state.phase == bj::Phase::PlayerTurn) {
        if (ImGui::Button("HIT [H]", {145, 45})) game.hit();
        ImGui::SameLine();
        if (ImGui::Button("STAND [S]", {145, 45})) game.stand();
    } else if (state.phase == bj::Phase::Ready) {
        if (ImGui::Button("DEAL HAND [SPACE]", {245, 45})) game.deal();
    } else if (state.phase == bj::Phase::RoundOver) {
        const ImVec4 color = state.outcome == bj::Outcome::Win ? green :
            (state.outcome == bj::Outcome::Loss ? red : gold);
        ImGui::TextColored(color, "%s  /  +%d points",
            state.outcome == bj::Outcome::Win ? "YOU WIN" :
            (state.outcome == bj::Outcome::Loss ? "HOUSE WINS" : "PUSH"), state.score.total);
        if (state.outcome == bj::Outcome::Win) {
            ImGui::SameLine();
            ImGui::TextColored(muted, "(%d + %d) x %.1f", state.score.base, state.score.bonus, state.score.multiplier);
        }
        if (ImGui::Button("CONTINUE [SPACE]", {245, 42})) game.advance();
    }
    if (state.phase != bj::Phase::Shop) ImGui::TextWrapped("%s", state.message.c_str());
    ImGui::End();

    if (state.phase == bj::Phase::Shop) {
        beginPanel("##shop", {w * 0.5f - 350, h * 0.5f - 255}, {700, 510}, false, true);
        heading("THE RELIC SHOP");
        ImGui::Text("Table cleared. Choose your next advantage.     Coins: %d", state.coins);
        const float rowStart = ImGui::GetCursorPosY();
        const float buttonX = ImGui::GetWindowSize().x - ImGui::GetStyle().WindowPadding.x - 155;
        int row = 0;
        for (const auto& relic : bj::relicCatalog()) {
            const float rowY = rowStart + static_cast<float>(row++) * 75;
            ImGui::PushID(static_cast<int>(relic.id));
            ImGui::SetCursorPos({20, rowY});
            ImGui::TextColored(gold, "%s", relic.name);
            ImGui::PushTextWrapPos(buttonX - 15);
            ImGui::TextWrapped("%s", relic.description);
            ImGui::PopTextWrapPos();
            ImGui::SetCursorPos({buttonX, rowY + 5});
            const bool owned = game.hasRelic(relic.id);
            ImGui::BeginDisabled(owned || game.state().coins < relic.cost);
            const std::string label = owned ? "Owned" : "Buy / " + std::to_string(relic.cost) + " coins";
            if (ImGui::Button(label.c_str(), {155, 35})) game.buy(relic.id);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::SetCursorPos({20, rowStart + static_cast<float>(row) * 75});
        ImGui::Separator();
        if (ImGui::Button("NEXT TABLE [SPACE]", {250, 40})) game.leaveShop();
        ImGui::End();
    }

    if (state.phase == bj::Phase::RunWon || state.phase == bj::Phase::RunLost) {
        beginPanel("##end", {w * 0.5f - 250, h * 0.5f - 140}, {500, 280}, false, true);
        heading(state.phase == bj::Phase::RunWon ? "THE HOUSE IS YOURS" : "THE HOUSE WINS THIS TIME");
        ImGui::TextWrapped("%s", state.message.c_str());
        ImGui::Text("Final table: %d     Points: %d / %d", state.stage, state.stageScore, state.goal);
        ImGui::Spacing();
        if (ImGui::Button("NEW RUN [SPACE]", {250, 45})) game.restart(state.seed + 1);
        ImGui::End();
    }

    if (debug) {
        ImGui::SetNextWindowPos({w * 0.5f - 200, 120}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize({400, 410}, ImGuiCond_Appearing);
        if (ImGui::Begin("Graphics settings", &debug)) {
            ImGui::Checkbox("Lighting", &settings.lighting);
            ImGui::Checkbox("Textures", &settings.textures);
            ImGui::Checkbox("Ambient motion", &settings.animate);
            ImGui::SliderFloat("Light", &settings.lightIntensity, 0.25f, 2.5f);
            ImGui::SliderFloat("Camera yaw", &settings.yaw, -35.0f, 35.0f);
            ImGui::SliderFloat("Camera pitch", &settings.pitch, 35.0f, 75.0f);
            ImGui::SliderFloat("Camera distance", &settings.distance, 7.0f, 14.0f);
            if (ImGui::Button("Reset camera")) settings = gfx::RenderSettings{};
            ImGui::Separator();
            ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
            ImGui::TextUnformatted("OpenGL 3.3 / GLSL 330 / Blinn-Phong");
        }
        ImGui::End();
    }
}

gfx::Scene makeScene(const bj::Snapshot& state, float time) {
    gfx::Scene scene;
    scene.time = time;
    scene.coins = state.coins;
    for (const auto& card : state.player)
        scene.player.push_back({card.rank, static_cast<int>(card.suit), false});
    for (std::size_t i = 0; i < state.dealer.size(); ++i) {
        const auto& card = state.dealer[i];
        scene.dealer.push_back({card.rank, static_cast<int>(card.suit),
            i == 1 && state.phase == bj::Phase::PlayerTurn});
    }
    return scene;
}

void autoplayStep(bj::Game& game) {
    switch (game.state().phase) {
        case bj::Phase::Ready: game.deal(); break;
        case bj::Phase::PlayerTurn:
            if (bj::evaluateHand(game.state().player).total < 17) game.hit();
            else game.stand();
            break;
        case bj::Phase::RoundOver: game.advance(); break;
        case bj::Phase::Shop:
            for (const auto id : {bj::Relic::Precision, bj::Relic::LongHand, bj::Relic::Streak, bj::Relic::SecondChance})
                if (game.buy(id)) break;
            game.leaveShop();
            break;
        default: game.restart(game.state().seed + 1); break;
    }
}

void saveScreenshot(const std::filesystem::path& path, int width, int height) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    stbi_flip_vertically_on_write(1);
    if (!stbi_write_png(path.string().c_str(), width, height, 3, pixels.data(), width * 3))
        throw std::runtime_error("Could not save screenshot: " + path.string());
    std::cout << "Screenshot: " << path.string() << '\n';
}
} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        Window window(options.hidden);
        Interface interface(window.handle);
        auto assets = options.assets;
        if (assets.empty()) assets = std::filesystem::absolute(argv[0]).parent_path() / "assets";
        if (!std::filesystem::exists(assets) && options.assets.empty()) assets = "assets";
        gfx::Renderer renderer(assets);
        gfx::RenderSettings settings;
        bj::Game game(options.seed);
        if (options.demo) game.deal();
        bool debug = false;
        int frame = 0;
        while (!glfwWindowShouldClose(window.handle)) {
            glfwPollEvents();
            int width = 0, height = 0;
            glfwGetFramebufferSize(window.handle, &width, &height);
            if (width == 0 || height == 0) {
                glfwWaitEventsTimeout(0.05);
                continue;
            }
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) glfwSetWindowShouldClose(window.handle, GLFW_TRUE);
            if (ImGui::IsKeyPressed(ImGuiKey_F1)) debug = !debug;
            if (!ImGui::GetIO().WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_H, false)) game.hit();
                if (ImGui::IsKeyPressed(ImGuiKey_S, false)) game.stand();
                // A focused UI control owns Space (e.g. buying a relic via keyboard).
                if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && !ImGui::GetIO().WantCaptureKeyboard)
                    primaryAction(game);
            }
            if (options.autoplay && frame % 10 == 0) {
                autoplayStep(game);
                std::cout << "Frame " << frame << ": stage=" << game.state().stage
                    << " phase=" << static_cast<int>(game.state().phase)
                    << " points=" << game.state().stageScore << '/' << game.state().goal << '\n';
            }
            drawInterface(game, settings, debug);
            renderer.render(makeScene(game.state(), static_cast<float>(glfwGetTime())), settings, width, height);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            const GLenum error = glGetError();
            if (error != GL_NO_ERROR) throw std::runtime_error("OpenGL error: " + std::to_string(error));
            ++frame;
            const bool finished = options.frames > 0 && frame >= options.frames;
            if (finished && !options.screenshot.empty()) saveScreenshot(options.screenshot, width, height);
            glfwSwapBuffers(window.handle);
            if (finished) break;
        }
        std::cout << "Rendered frames: " << frame << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Blackjack Forge: " << error.what() << '\n';
        return 1;
    }
}
