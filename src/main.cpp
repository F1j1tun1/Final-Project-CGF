#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

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
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr ImVec4 gold{0.98f, 0.78f, 0.34f, 1.0f};
constexpr ImVec4 muted{0.73f, 0.66f, 0.80f, 1.0f};
constexpr ImVec4 cream{0.98f, 0.93f, 0.81f, 1.0f};
constexpr ImVec4 green{0.54f, 0.88f, 0.72f, 1.0f};
constexpr ImVec4 red{1.0f, 0.48f, 0.49f, 1.0f};
constexpr ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoBringToFrontOnFocus;

std::uint32_t freshSeed() {
    std::random_device random;
    return random();
}

std::filesystem::path executableDirectory(const char* argument) {
#ifdef _WIN32
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length > 0 && length < path.size())
        return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> path(size);
    if (_NSGetExecutablePath(path.data(), &size) == 0)
        return std::filesystem::weakly_canonical(path.data()).parent_path();
#elif defined(__linux__)
    std::error_code error;
    const auto path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) return path.parent_path();
#endif
    return std::filesystem::absolute(argument).parent_path();
}

struct Options {
    std::uint32_t seed = 0;
    bool fixedSeed = false;
    int frames = 0;
    bool hidden = false;
    bool demo = false;
    bool autoplay = false;
    bool smoke = false;
    bool noAnimation = false;
    std::filesystem::path screenshot;
    std::filesystem::path captures;
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
            options.fixedSeed = true;
        } else if (arg == "--frames") {
            options.frames = std::stoi(value());
            if (options.frames < 1) throw std::runtime_error("Frames must be positive.");
        } else if (arg == "--screenshot") options.screenshot = value();
        else if (arg == "--capture-states") options.captures = value();
        else if (arg == "--assets") options.assets = value();
        else if (arg == "--hidden") options.hidden = true;
        else if (arg == "--demo") options.demo = true;
        else if (arg == "--autoplay") options.autoplay = true;
        else if (arg == "--no-animation") options.noAnimation = true;
        else if (arg == "--smoke") {
            options.hidden = true;
            options.autoplay = true;
            options.smoke = true;
            options.frames = 180;
        } else if (arg == "--help") {
            std::cout << "Jester Jack\n"
                "  --seed N          Reproducible run (otherwise a fresh random seed)\n"
                "  --demo            Skip menu and deal the first hand\n"
                "  --frames N        Exit after N rendered frames\n"
                "  --screenshot FILE Save the final frame as PNG\n"
                "  --capture-states DIR Save menu, table, shop and end screens\n"
                "  --assets DIR      Override assets directory\n"
                "  --hidden          Create an invisible graphics window\n"
                "  --no-animation    Skip the start fade\n"
                "  --autoplay        Exercise the game state machine\n"
                "  --smoke           Hidden integration run, including menu and resize\n";
            std::exit(0);
        } else throw std::runtime_error("Unknown option: " + arg);
    }
    if (!options.fixedSeed) options.seed = options.smoke ? 42 : freshSeed();
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
        handle = glfwCreateWindow(1440, 900, "Jester Jack", nullptr, nullptr);
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
    ImFont* title = nullptr;
    ImFont* heading = nullptr;
    explicit Interface(GLFWwindow* window, const std::filesystem::path& assets) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImFontConfig font;
        font.OversampleH = 3;
        font.OversampleV = 2;
        const auto regular = (assets / "fonts/CormorantGaramond-Regular.ttf").string();
        const auto semibold = (assets / "fonts/CormorantGaramond-SemiBold.ttf").string();
        io.FontDefault = io.Fonts->AddFontFromFileTTF(semibold.c_str(), 23.0f, &font);
        heading = io.Fonts->AddFontFromFileTTF(semibold.c_str(), 33.0f, &font);
        title = io.Fonts->AddFontFromFileTTF(regular.c_str(), 112.0f, &font);
        if (!io.FontDefault || !heading || !title) {
            ImGui::DestroyContext();
            throw std::runtime_error("Missing Jester Jack fonts. Keep assets beside the executable.");
        }
        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        style.WindowRounding = 9.0f;
        style.FrameRounding = 5.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.FramePadding = ImVec2(16, 8);
        style.WindowPadding = ImVec2(20, 16);
        style.ItemSpacing = ImVec2(12, 9);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.085f, 0.045f, 0.13f, 0.97f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.63f, 0.43f, 0.22f, 0.72f);
        style.Colors[ImGuiCol_Text] = cream;
        style.Colors[ImGuiCol_TextDisabled] = muted;
        style.Colors[ImGuiCol_Button] = ImVec4(0.30f, 0.10f, 0.30f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.48f, 0.20f, 0.36f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.57f, 0.26f, 0.34f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.12f, 0.29f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = gold;
        style.Colors[ImGuiCol_SliderGrab] = gold;
        style.Colors[ImGuiCol_SliderGrabActive] = cream;
        style.Colors[ImGuiCol_NavCursor] = gold;
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.17f, 0.08f, 0.23f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.28f, 0.10f, 0.29f, 1.0f);
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

struct Session {
    bool menu = true;
    bool settings = false;
    bool started = false;
    bool fixedSeed = false;
    bool transitions = true;
    float fade = -1.0f;
    bool switched = false;
    std::uint32_t nextSeed = 0;

    bool transitioning() const { return fade >= 0.0f; }
    void start(bj::Game& game) {
        if (transitioning()) return;
        nextSeed = !started ? game.state().seed :
            (fixedSeed ? game.state().seed + 1 : freshSeed());
        settings = false;
        switched = false;
        fade = 0.0f;
        if (!transitions) update(1.0f, game);
    }
    void update(float dt, bj::Game& game) {
        if (!transitioning()) return;
        fade += dt / 0.9f;
        if (fade >= 0.5f && !switched) {
            game.restart(nextSeed);
            menu = false;
            started = true;
            switched = true;
        }
        if (fade >= 1.0f) fade = -1.0f;
    }
    float darkness() const {
        return transitioning() ? std::clamp(1.0f - std::abs(2.0f * fade - 1.0f), 0.0f, 1.0f) : 0.0f;
    }
};

// At most one accepted game command per frame, shared by keyboard and buttons.
struct Commands {
    bool blocked = false;
    bool used = false;
    template <typename Action> void apply(Action action) {
        if (!blocked && !used && action()) used = true;
    }
};

void beginPanel(const char* name, ImVec2 position, ImVec2 size, bool transparent = false) {
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin(name, nullptr, panelFlags | (transparent ? ImGuiWindowFlags_NoBackground : 0));
}

void heading(const char* text, const Interface& ui) {
    ImGui::PushFont(ui.heading);
    ImGui::TextColored(gold, "%s", text);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();
}

const bj::RelicInfo& relicInfo(bj::Relic id) {
    const auto& catalog = bj::relicCatalog();
    return *std::find_if(catalog.begin(), catalog.end(), [id](const auto& item) { return item.id == id; });
}

void primaryAction(bj::Game& game, Commands& commands) {
    commands.apply([&]() {
        switch (game.state().phase) {
            case bj::Phase::Ready: return game.deal();
            case bj::Phase::RoundOver: return game.advance();
            case bj::Phase::Shop: return game.leaveShop();
            default: return false;
        }
    });
}

void drawMenu(bj::Game& game, Session& session, const Interface& ui, GLFWwindow* window) {
    const auto display = ImGui::GetIO().DisplaySize;
    auto* backdrop = ImGui::GetBackgroundDrawList();
    backdrop->AddRectFilledMultiColor({0, 0}, display, IM_COL32(14, 7, 25, 100),
        IM_COL32(14, 7, 25, 0), IM_COL32(14, 7, 25, 20), IM_COL32(14, 7, 25, 210));
    const char* name = "JESTER JACK";
    const float textWidth = ui.title->CalcTextSizeA(112.0f, 10000, 0, name).x;
    const float size = 112.0f * std::min(1.0f, display.x * 0.58f / textWidth);
    const auto titleSize = ui.title->CalcTextSizeA(size, 10000, 0, name);
    const float right = display.x - 55.0f;
    const ImVec2 titlePosition{right - titleSize.x, display.y * 0.06f};
    backdrop->AddText(ui.title, size, {titlePosition.x + 3, titlePosition.y + 4}, IM_COL32(12, 4, 18, 255), name);
    backdrop->AddText(ui.title, size, titlePosition, ImGui::ColorConvertFloat4ToU32(gold), name);
    backdrop->AddLine({titlePosition.x + 6, titlePosition.y + titleSize.y + 7},
        {right, titlePosition.y + titleSize.y + 7}, IM_COL32(213, 158, 68, 180), 1.0f);
    beginPanel("##invitation", {display.x * 0.08f, display.y * 0.40f}, {350, 150}, true);
    ImGui::PushFont(ui.heading);
    ImGui::TextColored(cream, "A fool's fortune.");
    ImGui::PopFont();
    ImGui::TextColored(muted, "Blackjack, with a twist.");
    ImGui::End();

    const float buttonWidth = 190;
    beginPanel("##menu", {display.x * 0.5f - 313, display.y - 148}, {626, 104}, true);
    ImGui::BeginDisabled(session.transitioning() || session.settings);
    if (ImGui::Button("START", {buttonWidth, 56})) session.start(game);
    ImGui::SameLine();
    if (ImGui::Button("SETTINGS", {buttonWidth, 56})) session.settings = true;
    ImGui::SameLine();
    if (ImGui::Button("EXIT", {buttonWidth, 56})) glfwSetWindowShouldClose(window, GLFW_TRUE);
    ImGui::EndDisabled();
    ImGui::End();
    const char* footer = "No wagers. One loss ends the run.";
    const auto footerSize = ImGui::CalcTextSize(footer);
    backdrop->AddText({(display.x - footerSize.x) * 0.5f, display.y - 38},
        ImGui::ColorConvertFloat4ToU32(muted), footer);
}

void drawShop(bj::Game& game, Commands& commands, const Interface& ui) {
    const auto state = game.state();
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled({0, 100}, display, IM_COL32(10, 4, 19, 165));
    const float width = std::min(760.0f, display.x - 100);
    const float height = std::min(630.0f, display.y - 124);
    beginPanel("##shop", {(display.x - width) * 0.5f, std::max(100.0f, (display.y - height) * 0.5f)}, {width, height});
    heading("THE JESTER'S WARES", ui);
    ImGui::Text("Coins: %d     Relics: %d / 4", state.coins, static_cast<int>(state.owned.size()));
    ImGui::TextColored(muted, "Two offers. Choose your next trick.");
    const float offerY = ImGui::GetCursorPosY() + 5;
    const float cardWidth = (width - 54) * 0.5f;
    for (std::size_t i = 0; i < state.offers.size(); ++i) {
        const auto& item = relicInfo(state.offers[i]);
        ImGui::PushID(static_cast<int>(item.id));
        const float x = 20 + static_cast<float>(i) * (cardWidth + 14);
        ImGui::SetCursorPos({x, offerY});
        ImGui::BeginChild("offer", {cardWidth, 206}, ImGuiChildFlags_Borders);
        ImGui::PushFont(ui.heading);
        ImGui::TextColored(gold, "%s", item.name);
        ImGui::PopFont();
        ImGui::TextWrapped("%s", item.description);
        ImGui::SetCursorPosY(139);
        const bool owned = game.hasRelic(item.id);
        const bool purchased = game.boughtThisShop(item.id);
        ImGui::BeginDisabled(commands.blocked || commands.used || owned ||
            purchased || game.state().owned.size() >= 4 || game.state().coins < item.cost);
        const std::string label = owned ? "PURCHASED" : purchased ? "SOLD" :
            "BUY / " + std::to_string(item.cost) + " COINS";
        if (ImGui::Button(label.c_str(), {-1, 43})) commands.apply([&]() { return game.buy(item.id); });
        ImGui::EndDisabled();
        ImGui::EndChild();
        ImGui::PopID();
    }
    ImGui::SetCursorPos({20, offerY + 219});
    ImGui::Separator();
    ImGui::TextColored(muted, "YOUR RELICS   /   Sell for half the purchase price");
    const float inventoryY = ImGui::GetCursorPosY();
    if (state.owned.empty()) ImGui::TextDisabled("Your pockets are empty. For now.");
    for (std::size_t i = 0; i < state.owned.size(); ++i) {
        const auto& item = relicInfo(state.owned[i]);
        ImGui::PushID(100 + static_cast<int>(item.id));
        ImGui::SetCursorPos({20, inventoryY + static_cast<float>(i) * 38});
        ImGui::TextColored(cream, "%s", item.name);
        ImGui::SameLine(width - 190);
        ImGui::BeginDisabled(commands.blocked || commands.used);
        const std::string label = "SELL / " + std::to_string(item.cost / 2);
        if (ImGui::Button(label.c_str(), {150, 32})) commands.apply([&]() { return game.sell(item.id); });
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::SetCursorPos({20, height - 70});
    ImGui::BeginDisabled(commands.blocked || commands.used);
    if (ImGui::Button("NEXT ROUND [SPACE]", {280, 44})) commands.apply([&]() { return game.leaveShop(); });
    ImGui::EndDisabled();
    ImGui::End();
}

void drawTable(bj::Game& game, Commands& commands, Session& session, const Interface& ui) {
    const auto state = game.state();
    const auto display = ImGui::GetIO().DisplaySize;
    const float w = display.x, h = display.y;
    const float side = w < 1250 ? 205.0f : 240.0f;
    beginPanel("##header", {24, 12}, {w - 48, 86}, true);
    ImGui::PushFont(ui.heading);
    ImGui::TextColored(gold, "JESTER JACK");
    ImGui::PopFont();
    ImGui::SameLine(290);
    ImGui::TextColored(muted, "THE FOOL'S TABLE");
    ImGui::SetCursorPos({w - 360, 18});
    ImGui::BeginDisabled(session.transitioning() || session.settings);
    if (ImGui::Button("MENU")) session.menu = true;
    ImGui::SameLine();
    if (ImGui::Button("SETTINGS [F1]")) session.settings = true;
    ImGui::EndDisabled();
    ImGui::End();

    if (state.phase == bj::Phase::Shop) {
        drawShop(game, commands, ui);
        return;
    }
    if (state.phase == bj::Phase::RunLost) {
        beginPanel("##end", {w * 0.5f - 270, h * 0.5f - 170}, {540, 340});
        heading("THE JOKE'S ON YOU", ui);
        ImGui::TextWrapped("The house wins this hand. Your run is over.");
        ImGui::Spacing();
        ImGui::Text("Rounds played: %d     Wins: %d", state.round, state.wins);
        ImGui::Text("Final score: %d     Coins: %d", state.totalScore, state.coins);
        ImGui::TextColored(muted, "Seed: %u", state.seed);
        ImGui::Spacing();
        ImGui::BeginDisabled(session.transitioning() || session.settings);
        if (ImGui::Button("NEW RUN [SPACE]", {245, 44})) session.start(game);
        ImGui::SameLine();
        if (ImGui::Button("MENU", {160, 44})) session.menu = true;
        ImGui::EndDisabled();
        ImGui::End();
        return;
    }

    beginPanel("##run", {24, 112}, {side, 298});
    heading("THE RUN", ui);
    ImGui::Text("Round        %d", state.round);
    ImGui::Text("Wins          %d", state.wins);
    ImGui::Text("Score         %d", state.totalScore);
    ImGui::TextColored(gold, "Coins         %d", state.coins);
    ImGui::Spacing();
    ImGui::TextWrapped("One loss ends the run. A push keeps you playing.");
    ImGui::End();

    beginPanel("##guide", {24, 428}, {side, h - 452});
    heading("THE RULES", ui);
    ImGui::TextWrapped(h < 820 ? "Closest to 21 wins. Dealer stands on 17." :
        "Get closer to 21 than the dealer. Aces count as 1 or 11.");
    if (h >= 820) ImGui::TextWrapped("Dealer stands on 17. Win to earn 3 coins, plus relic bonuses.");
    ImGui::TextColored(muted, "H  /  Hit");
    ImGui::TextColored(muted, "S  /  Stand");
    ImGui::TextColored(muted, "Space  /  Continue");
    ImGui::End();

    beginPanel("##relics", {w - side - 24, 112}, {side, h - 148});
    heading("YOUR RELICS", ui);
    ImGui::TextColored(muted, "%d / 4 equipped", static_cast<int>(state.owned.size()));
    if (state.owned.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("A little luck. A little mischief. Visit the shop after a round.");
    }
    for (const auto id : state.owned) {
        const auto& item = relicInfo(id);
        ImGui::Spacing();
        ImGui::TextColored(gold, "%s", item.name);
        ImGui::TextWrapped("%s", item.description);
        if (id == bj::Relic::SecondChance)
            ImGui::TextColored(muted, "%s", state.usedSecondChance ? "Spent this run" : "One rescue ready");
        ImGui::Separator();
    }
    ImGui::End();

    const float centerWidth = w - 2 * side - 100;
    beginPanel("##dealer", {side + 50, 110}, {centerWidth, 80}, true);
    if (state.dealer.empty()) ImGui::TextColored(muted, "DEALER  /  Your seat is ready");
    else if (state.phase == bj::Phase::PlayerTurn)
        ImGui::Text("DEALER  /  %d + hidden card", bj::evaluateHand({state.dealer.front()}).total);
    else ImGui::Text("DEALER  /  %d", bj::evaluateHand(state.dealer).total);
    ImGui::End();

    beginPanel("##actions", {side + 50, h - 216}, {centerWidth, 192});
    const auto hand = bj::evaluateHand(state.player);
    ImGui::TextColored(muted, "YOUR HAND");
    ImGui::SameLine();
    if (state.player.empty()) ImGui::TextUnformatted("--");
    else ImGui::TextColored(hand.bust ? red : green, "%d%s", hand.total, hand.soft ? "  (soft)" : "");
    ImGui::BeginDisabled(commands.blocked || commands.used);
    if (state.phase == bj::Phase::PlayerTurn) {
        if (ImGui::Button("HIT [H]", {145, 44})) commands.apply([&]() { return game.hit(); });
        ImGui::SameLine();
        if (ImGui::Button("STAND [S]", {145, 44})) commands.apply([&]() { return game.stand(); });
    } else if (state.phase == bj::Phase::Ready) {
        if (ImGui::Button("DEAL HAND [SPACE]", {245, 44})) commands.apply([&]() { return game.deal(); });
    } else if (state.phase == bj::Phase::RoundOver) {
        const ImVec4 color = state.outcome == bj::Outcome::Win ? green :
            (state.outcome == bj::Outcome::Loss ? red : gold);
        ImGui::TextColored(color, "%s  /  +%d coins", state.outcome == bj::Outcome::Win ? "YOU WIN" :
            (state.outcome == bj::Outcome::Loss ? "HOUSE WINS" : "PUSH"), state.score.coins);
        if (ImGui::Button(state.outcome == bj::Outcome::Loss ? "END RUN [SPACE]" : "SHOP [SPACE]", {245, 40}))
            commands.apply([&]() { return game.advance(); });
    }
    ImGui::EndDisabled();
    if (state.phase != bj::Phase::Shop) ImGui::TextWrapped("%s", state.message.c_str());
    ImGui::End();

}

void drawSettings(Session& session, gfx::RenderSettings& settings, const Interface& ui) {
    if (!session.settings) return;
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x * 0.5f - 235, display.y * 0.5f - 310});
    ImGui::SetNextWindowSize({470, 620});
    ImGui::SetNextWindowFocus();
    if (ImGui::Begin("SETTINGS", &session.settings, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
        heading("SETTINGS", ui);
        ImGui::Checkbox("Lighting", &settings.lighting);
        ImGui::Checkbox("Textures", &settings.textures);
        ImGui::Checkbox("Ambient motion", &settings.animate);
        ImGui::Checkbox("Start transition", &session.transitions);
        ImGui::SliderFloat("Light", &settings.lightIntensity, 0.25f, 2.5f);
        ImGui::SliderFloat("Camera yaw", &settings.yaw, -35.0f, 35.0f);
        ImGui::SliderFloat("Camera pitch", &settings.pitch, 35.0f, 75.0f);
        ImGui::SliderFloat("Camera distance", &settings.distance, 7.0f, 14.0f);
        if (ImGui::Button("RESET VIEW")) settings = gfx::RenderSettings{};
        ImGui::SameLine();
        if (ImGui::Button("DONE")) session.settings = false;
    }
    ImGui::End();
}

gfx::Scene makeScene(const bj::Snapshot& state, float time, bool menu) {
    gfx::Scene scene;
    scene.menu = menu;
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

void autoplayStep(bj::Game& game, Commands& commands) {
    if (game.state().phase == bj::Phase::Shop) {
        for (const auto id : game.state().offers) {
            commands.apply([&]() { return game.buy(id); });
            if (commands.used) return;
        }
        commands.apply([&]() { return game.leaveShop(); });
    } else if (game.state().phase == bj::Phase::PlayerTurn) {
        commands.apply([&]() {
            return bj::evaluateHand(game.state().player).total < 17 ? game.hit() : game.stand();
        });
    } else primaryAction(game, commands);
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
        auto assets = options.assets;
        if (assets.empty()) assets = executableDirectory(argv[0]) / "assets";
        if (!std::filesystem::exists(assets) && options.assets.empty()) assets = "assets";
        Window window(options.hidden);
        Interface interface(window.handle, assets);
        gfx::Renderer renderer(assets);
        gfx::RenderSettings settings;
        bj::Game game(options.seed);
        Session session;
        session.fixedSeed = options.fixedSeed || options.smoke;
        session.transitions = !options.noAnimation;
        if (options.demo) {
            session.menu = false;
            session.started = true;
            game.deal();
        }
        int frame = 0;
        double previousTime = glfwGetTime();
        std::set<std::string> captured;
        while (!glfwWindowShouldClose(window.handle)) {
            glfwPollEvents();
            const double now = glfwGetTime();
            const float dt = options.hidden && options.frames > 0 ? 1.0f / 60.0f :
                static_cast<float>(std::min(now - previousTime, 0.1));
            previousTime = now;
            int width = 0, height = 0;
            glfwGetFramebufferSize(window.handle, &width, &height);
            if (width == 0 || height == 0) {
                glfwWaitEventsTimeout(0.05);
                continue;
            }
            session.update(dt, game);
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            if (options.smoke) {
                auto& io = ImGui::GetIO();
                if (frame == 1 || frame == 6) io.AddKeyEvent(ImGuiKey_F1, true);
                if (frame == 2 || frame == 7) io.AddKeyEvent(ImGuiKey_F1, false);
                if (!options.demo && (frame == 10 || frame == 11)) {
                    io.AddMousePosEvent(io.DisplaySize.x * 0.5f - 198, io.DisplaySize.y - 104);
                    io.AddMouseButtonEvent(0, frame == 10);
                }
            }
            ImGui::NewFrame();
            Commands commands{session.menu || session.settings || session.transitioning()};
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                if (session.settings) session.settings = false;
                else if (!session.transitioning()) session.menu = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F1, false) && !session.transitioning())
                session.settings = !session.settings;
            commands.blocked = session.menu || session.settings || session.transitioning();
            if (!ImGui::GetIO().WantTextInput && !session.settings && !session.transitioning()) {
                if (ImGui::IsKeyPressed(ImGuiKey_H, false)) commands.apply([&]() { return game.hit(); });
                if (ImGui::IsKeyPressed(ImGuiKey_S, false)) commands.apply([&]() { return game.stand(); });
                if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && !ImGui::GetIO().WantCaptureKeyboard) {
                    if (session.menu || game.state().phase == bj::Phase::RunLost) session.start(game);
                    else primaryAction(game, commands);
                }
            }
            // The smoke path traverses the real menu/fade and changes the window size.
            if (options.smoke && frame == 100) glfwSetWindowSize(window.handle, 1100, 720);
            if (options.smoke && frame == 160) glfwSetWindowSize(window.handle, 1440, 900);
            if (options.autoplay && frame > 0 && frame % 10 == 0 && !session.transitioning() && !session.settings) {
                if ((session.menu && !options.smoke) || game.state().phase == bj::Phase::RunLost) session.start(game);
                else autoplayStep(game, commands);
                std::cout << "Frame " << frame << ": round=" << game.state().round
                    << " phase=" << static_cast<int>(game.state().phase)
                    << " coins=" << game.state().coins << " seed=" << game.state().seed << '\n';
            }
            if (session.menu) drawMenu(game, session, interface, window.handle);
            else drawTable(game, commands, session, interface);
            drawSettings(session, settings, interface);
            if (options.smoke && ((frame == 2 && !session.settings) || (frame == 8 && session.settings) ||
                (!options.demo && frame == 12 && (options.noAnimation ? session.menu : !session.transitioning()))))
                throw std::runtime_error("Smoke input failed: Settings/Start did not respond.");
            const float sceneTime = options.hidden && options.frames > 0 ? frame / 60.0f : static_cast<float>(now);
            renderer.render(makeScene(game.state(), sceneTime, session.menu), settings, width, height);
            if (session.transitioning()) ImGui::GetForegroundDrawList()->AddRectFilled(
                {0, 0}, ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, static_cast<int>(255 * session.darkness())));
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            const GLenum error = glGetError();
            if (error != GL_NO_ERROR) throw std::runtime_error("OpenGL error: " + std::to_string(error));
            if (!options.captures.empty() && !session.transitioning() && !commands.used) {
                const std::string name = session.settings ? "settings" : session.menu ? "menu" :
                    game.state().phase == bj::Phase::Shop ? "shop" :
                    game.state().phase == bj::Phase::RunLost ? "end" : "table";
                if (captured.insert(name).second) saveScreenshot(options.captures / (name + ".png"), width, height);
            }
            if (options.smoke && !options.captures.empty() && (frame == 25 || frame == 38 || frame == 51))
                saveScreenshot(options.captures / ("fade-" + std::to_string(frame) + ".png"), width, height);
            ++frame;
            const bool finished = options.frames > 0 && frame >= options.frames;
            if (finished && !options.screenshot.empty()) saveScreenshot(options.screenshot, width, height);
            glfwSwapBuffers(window.handle);
            if (finished) break;
        }
        std::cout << "Rendered frames: " << frame << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Jester Jack: " << error.what() << '\n';
        return 1;
    }
}
