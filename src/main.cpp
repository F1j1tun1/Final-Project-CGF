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
#include "presentation/CardSuit.hpp"

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
constexpr ImVec4 blue{0.30f, 0.72f, 1.0f, 1.0f};
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
    ImFont* button = nullptr;
    ImFont* menuButton = nullptr;
    ImFont* number = nullptr;
    ImFont* largeNumber = nullptr;
    ImFont* small = nullptr;
    explicit Interface(GLFWwindow* window, const std::filesystem::path& assets) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImFontConfig font;
        font.OversampleH = 3;
        font.OversampleV = 2;
        const auto semibold = (assets / "fonts/CormorantGaramond-SemiBold.ttf").string();
        const auto body = (assets / "fonts/SourceSans3-Regular.ttf").string();
        const auto bold = (assets / "fonts/SourceSans3-Semibold.ttf").string();
        io.FontDefault = io.Fonts->AddFontFromFileTTF(body.c_str(), 22.0f, &font);
        heading = io.Fonts->AddFontFromFileTTF(semibold.c_str(), 36.0f, &font);
        title = io.Fonts->AddFontFromFileTTF(semibold.c_str(), 200.0f, &font);
        button = io.Fonts->AddFontFromFileTTF(bold.c_str(), 26.0f, &font);
        menuButton = io.Fonts->AddFontFromFileTTF(bold.c_str(), 34.0f, &font);
        number = io.Fonts->AddFontFromFileTTF(bold.c_str(), 46.0f, &font);
        largeNumber = io.Fonts->AddFontFromFileTTF(bold.c_str(), 104.0f, &font);
        small = io.Fonts->AddFontFromFileTTF(body.c_str(), 18.0f, &font);
        if (!io.FontDefault || !heading || !title || !button || !menuButton || !number || !largeNumber || !small) {
            ImGui::DestroyContext();
            throw std::runtime_error("Missing Jester Jack fonts. Keep assets beside the executable.");
        }
        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        style.WindowRounding = 12.0f;
        style.FrameRounding = 7.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.FramePadding = ImVec2(16, 8);
        style.WindowPadding = ImVec2(20, 16);
        style.ItemSpacing = ImVec2(12, 9);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.095f, 0.11f, 0.98f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.29f, 0.35f, 0.37f, 0.96f);
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
    bool rules = false;
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
        rules = false;
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

struct HandFeedback {
    std::vector<bj::Card> previous;
    std::uint32_t seed = 0;
    int round = 0;
    int handsLeft = -1;
    bool usedSecondChance = false;
    bj::Card card{1, bj::Suit::Clubs};
    int delta = 0;
    float remaining = 0;

    void observe(const bj::Snapshot& state, float dt, bool visible) {
        remaining = std::max(0.0f, remaining - dt);
        const bool sameHand = seed == state.seed && round == state.round && handsLeft == state.handsLeft;
        const bool appended = state.player.size() == previous.size() + 1 && !previous.empty() &&
            std::equal(previous.begin(), previous.end(), state.player.begin(), [](const bj::Card& a, const bj::Card& b) {
                return a.rank == b.rank && a.suit == b.suit;
            });
        if (!visible || !sameHand || state.phase == bj::Phase::Ready || state.phase == bj::Phase::Shop ||
            state.player.empty() || usedSecondChance != state.usedSecondChance) {
            remaining = 0;
        } else if (appended) {
            card = state.player.back();
            delta = bj::evaluateHand(state.player).total - bj::evaluateHand(previous).total;
            remaining = 1.5f;
        }
        previous = state.player;
        seed = state.seed;
        round = state.round;
        handsLeft = state.handsLeft;
        usedSecondChance = state.usedSecondChance;
    }
};

void beginPanel(const char* name, ImVec2 position, ImVec2 size, bool transparent = false,
                ImGuiWindowFlags extraFlags = 0) {
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin(name, nullptr, panelFlags | extraFlags | (transparent ? ImGuiWindowFlags_NoBackground : 0));
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
            case bj::Phase::PlayerTurn: return game.stand();
            case bj::Phase::RoundOver: return game.advance();
            case bj::Phase::Shop: return game.leaveShop();
            default: return false;
        }
    });
}

struct MenuLayout {
    ImVec2 origin;
    float width = 350;
    float height = 86;
    float step = 102;
};

MenuLayout menuLayout(ImVec2 display) {
    return {{display.x * 0.075f, display.y * 0.52f}};
}

void drawMenu(bj::Game& game, Session& session, const Interface& ui, GLFWwindow* window) {
    const auto display = ImGui::GetIO().DisplaySize;
    auto* backdrop = ImGui::GetBackgroundDrawList();
    backdrop->AddRectFilledMultiColor({0, 0}, {display.x * 0.48f, display.y},
        IM_COL32(27, 15, 32, 125), IM_COL32(27, 15, 32, 0),
        IM_COL32(27, 15, 32, 0), IM_COL32(27, 15, 32, 155));
    const char* name = "Jester Jack";
    const float textWidth = ui.title->CalcTextSizeA(200.0f, 10000, 0, name).x;
    const float size = 200.0f * std::min(1.0f, display.x * 0.76f / textWidth);
    const ImVec2 titlePosition{display.x * 0.07f, display.y * 0.055f};
    backdrop->AddText(ui.title, size, {titlePosition.x + 3, titlePosition.y + 4}, IM_COL32(35, 17, 27, 180), name);
    backdrop->AddText(ui.title, size, titlePosition, ImGui::ColorConvertFloat4ToU32(cream), name);

    const auto layout = menuLayout(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    beginPanel("##menu", layout.origin, {layout.width, 3 * layout.step}, true);
    ImGui::BeginDisabled(session.transitioning() || session.settings);
    ImGui::PushFont(ui.menuButton);
    const auto button = [&](const char* label, ImVec4 normal, ImVec4 hover, ImVec4 active) {
        ImGui::PushStyleColor(ImGuiCol_Button, normal);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
        const bool pressed = ImGui::Button(label, {layout.width, layout.height});
        ImGui::PopStyleColor(3);
        return pressed;
    };
    if (button("START", {.13f, .40f, .23f, 1}, {.19f, .53f, .31f, 1}, {.10f, .32f, .18f, 1}))
        session.start(game);
    ImGui::SetCursorPos({0, layout.step});
    if (button("OPTIONS", {.29f, .31f, .34f, 1}, {.40f, .43f, .46f, 1}, {.22f, .24f, .27f, 1}))
        session.settings = true;
    ImGui::SetCursorPos({0, 2 * layout.step});
    if (button("EXIT", {.56f, .16f, .20f, 1}, {.70f, .23f, .27f, 1}, {.44f, .11f, .14f, 1}))
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    ImGui::PopFont();
    ImGui::EndDisabled();
    ImGui::End();
    ImGui::PopStyleVar();
    backdrop->AddText({layout.origin.x, display.y - 49}, IM_COL32(234, 213, 190, 225),
        "Five hands. Reach the target.");
}

ImVec4 rarityColor(bj::Rarity rarity) {
    switch (rarity) {
        case bj::Rarity::Common: return {0.77f, 0.81f, 0.79f, 1};
        case bj::Rarity::Uncommon: return {0.35f, 0.84f, 0.54f, 1};
        case bj::Rarity::Rare: return {0.32f, 0.67f, 1.0f, 1};
        case bj::Rarity::Epic: return {0.79f, 0.47f, 1.0f, 1};
        case bj::Rarity::Legendary: return {1.0f, 0.70f, 0.25f, 1};
    }
    return cream;
}

void rarityBadge(bj::Rarity rarity, const Interface& ui) {
    const auto color = ImGui::ColorConvertFloat4ToU32(rarityColor(rarity));
    const char* label = bj::rarityName(rarity);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 text = ui.button->CalcTextSizeA(26, 1000, 0, label);
    ImGui::Dummy({text.x + 20, 32});
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, {origin.x + text.x + 20, origin.y + 32}, color, 5);
    draw->AddText(ui.button, 26, {origin.x + 10, origin.y + 3}, IM_COL32(17, 22, 28, 255), label);
}

void drawCoinIcon(ImDrawList* draw, ImVec2 center, float radius, const Interface& ui) {
    const ImU32 rim = IM_COL32(166, 103, 27, 255);
    draw->AddCircleFilled({center.x - 7, center.y + 4}, radius, rim, 32);
    draw->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(gold), 32);
    draw->AddCircle(center, radius - 4, rim, 32, 2);
    const ImVec2 mark = ui.button->CalcTextSizeA(26, 1000, 0, "$");
    draw->AddText(ui.button, 26, {center.x - mark.x * .5f, center.y - mark.y * .5f}, rim, "$");
}

const char* relicSummary(bj::Relic relic) {
    switch (relic) {
        case bj::Relic::Precision: return "21 win: +5 pts / +1 coin";
        case bj::Relic::LongHand: return "4+ cards: +8 pts / +1 coin";
        case bj::Relic::SecondChance: return "1 bust rescue per round";
        case bj::Relic::Streak: return "Win streak: up to x1.4";
        case bj::Relic::RoyalPurse: return "Blackjack: +1 coin";
        case bj::Relic::SoftTouch: return "Soft win: +1 coin";
        case bj::Relic::NarrowEscape: return "Dealer bust: +3 pts / +1 coin";
        case bj::Relic::RedRibbon: return "Red pair win: +1 coin";
        case bj::Relic::BlackVelvet: return "Black pair win: +1 coin";
        case bj::Relic::EvenOdds: return "Even total win: +1 coin";
        case bj::Relic::CourtFavor: return "J / Q / K win: +3 pts";
        case bj::Relic::HumbleCrown: return "Win at 18 or less: +5 pts";
        case bj::Relic::BlackjackBonus: return "Natural blackjack: +5 pts";
    }
    return "";
}

struct TableLayout {
    float sidebar;
    float contentX;
    float contentWidth;
    float boardY;
    float boardHeight;
    float actionY;
};

TableLayout tableLayout(ImVec2 display) {
    const float sidebar = std::clamp(display.x * .24f, 280.0f, 340.0f);
    const float contentX = sidebar + 24;
    const float actionY = display.y - 154;
    return {sidebar, contentX, display.x - contentX - 20, 202, actionY - 212, actionY};
}

float controlsLeft(const TableLayout& layout) {
    constexpr float width = 456;
    const float tableCenter = layout.contentX + layout.contentWidth * .5f;
    const float handCenter = layout.contentX + layout.contentWidth - 105;
    // Leave room for two large hand digits at the minimum window width.
    const float center = std::min(tableCenter, handCenter - 80 - width * .5f);
    return center - width * .5f;
}

ImVec2 dealButtonCenter(const TableLayout& layout) {
    return {controlsLeft(layout) + 228, layout.actionY + 81};
}

void relicTooltip(const bj::RelicInfo& item, const Interface& ui) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20);
    ImGui::TextColored(rarityColor(item.rarity), "%s / %s", item.name, bj::rarityName(item.rarity));
    ImGui::TextWrapped("%s", item.description);
    ImGui::PushFont(ui.small);
    ImGui::Text("Buy: %d coins    Sell: %d coins", item.cost, item.cost / 2);
    ImGui::PopFont();
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

void drawRelicStrip(const bj::Game& game, const TableLayout& layout, const Interface& ui) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    beginPanel("##relics", {layout.contentX, 16}, {layout.contentWidth, 174}, true);
    ImGui::TextColored(cream, "RELICS  %d / 4", static_cast<int>(game.state().owned.size()));
    ImGui::SameLine();
    ImGui::PushFont(ui.small);
    ImGui::TextColored(muted, "Hover for effects");
    ImGui::PopFont();
    const float width = (layout.contentWidth - 36) / 4;
    for (int slot = 0; slot < 4; ++slot) {
        ImGui::PushID(slot);
        ImGui::SetCursorPos({static_cast<float>(slot) * (width + 12), 34});
        const bool filled = static_cast<std::size_t>(slot) < game.state().owned.size();
        const auto* item = filled ? &relicInfo(game.state().owned[static_cast<std::size_t>(slot)]) : nullptr;
        const ImVec4 accent = filled ? rarityColor(item->rarity) : ImVec4(.27f, .31f, .34f, 1);
        ImGui::PushStyleColor(ImGuiCol_Border, accent);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, filled
            ? ImVec4(.07f + accent.x * .16f, .08f + accent.y * .16f, .10f + accent.z * .16f, .99f)
            : ImVec4(.105f, .12f, .15f, .97f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, filled ? 2.0f : 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12, 10});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4, 3});
        ImGui::BeginChild("slot", {width, 140}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        if (item) {
            const ImVec2 namePosition = ImGui::GetCursorScreenPos();
            const float available = ImGui::GetContentRegionAvail().x;
            const float nameWidth = ui.button->CalcTextSizeA(26, 1000, 0, item->name).x;
            const float nameSize = 26 * std::min(1.0f, available / nameWidth);
            ImGui::Dummy({available, 26});
            ImGui::GetWindowDrawList()->AddText(ui.button, nameSize, namePosition,
                ImGui::ColorConvertFloat4ToU32(cream), item->name);
            rarityBadge(item->rarity, ui);
            ImGui::PushFont(ui.small);
            ImGui::TextWrapped("%s", item->id == bj::Relic::SecondChance && game.state().usedSecondChance
                ? "Rescue used this round" : relicSummary(item->id));
            ImGui::PopFont();
        } else {
            ImGui::SetCursorPosY(49);
            ImGui::PushFont(ui.small);
            ImGui::TextColored(muted, "Empty relic slot");
            ImGui::PopFont();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
        if (item && ImGui::IsItemHovered()) relicTooltip(*item, ui);
        ImGui::PopID();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void drawSidebar(const bj::Snapshot& state, const TableLayout& layout, Session& session, const Commands& commands,
                 const Interface& ui, float height) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16, 14});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8, 8});
    beginPanel("##run", {16, 16}, {layout.sidebar - 16, height - 32});
    ImGui::PushFont(ui.button);
    ImGui::TextColored(gold, "ROUND %d", state.round);
    ImGui::PopFont();
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, {.23f, .13f, .15f, 1});
    ImGui::BeginChild("target", {-1, 128}, ImGuiChildFlags_Borders);
    ImGui::PushFont(ui.small);
    ImGui::TextColored(cream, "SCORE AT LEAST");
    ImGui::PopFont();
    ImGui::PushFont(ui.number);
    ImGui::TextColored(red, "%d", state.goal);
    ImGui::PopFont();
    ImGui::PushFont(ui.small);
    ImGui::TextColored(muted, "points to clear this round");
    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, {.075f, .16f, .22f, 1});
    ImGui::BeginChild("score", {-1, 122}, ImGuiChildFlags_Borders);
    ImGui::PushFont(ui.small);
    ImGui::TextColored(cream, "ROUND SCORE");
    ImGui::PopFont();
    ImGui::PushFont(ui.number);
    ImGui::TextColored(blue, "%d", state.roundScore);
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, blue);
    ImGui::ProgressBar(std::min(1.0f, static_cast<float>(state.roundScore) / state.goal), {-1, 7}, "");
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    const float half = (ImGui::GetContentRegionAvail().x - 8) * .5f;
    for (int column = 0; column < 2; ++column) {
        if (column) ImGui::SameLine();
        ImGui::PushID(column);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, column ? ImVec4(.22f, .17f, .08f, 1) : ImVec4(.08f, .17f, .21f, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {14, 8});
        ImGui::BeginChild("counter", {half, 92}, ImGuiChildFlags_Borders);
        ImGui::PushFont(ui.small);
        ImGui::TextUnformatted(column ? "COINS" : "HANDS LEFT");
        ImGui::PopFont();
        ImGui::PushFont(ui.number);
        ImGui::TextColored(column ? gold : blue, "%d", column ? state.coins : state.handsLeft);
        ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    if (height >= 820) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {14, 8});
        ImGui::BeginChild("hand-reward", {-1, 94}, ImGuiChildFlags_Borders);
        ImGui::PushFont(ui.small);
        ImGui::TextColored(muted, "HAND REWARD");
        ImGui::PopFont();
        ImGui::TextColored(green, "+%d points  /  +%d coins", state.score.total, state.score.coins);
        ImGui::PushFont(ui.small);
        ImGui::TextColored(muted, "(%d + %d)  x  %.1f", state.score.base, state.score.bonus, state.score.multiplier);
        ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::PushFont(ui.small);
    ImGui::TextColored(muted, "Run score: %d", state.totalScore);
    ImGui::TextColored(muted, "%d more points needed", std::max(0, state.goal - state.roundScore));
    ImGui::PopFont();

    ImGui::SetCursorPosY(height - 32 - 108);
    ImGui::BeginDisabled(commands.used || session.settings || session.rules || session.transitioning());
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {12, 4});
    if (ImGui::Button("HOW TO PLAY", {-1, 36})) session.rules = true;
    ImGui::PopStyleVar();
    const float buttonWidth = (ImGui::GetContentRegionAvail().x - 8) * .5f;
    ImGui::PushFont(ui.button);
    if (ImGui::Button("MENU", {buttonWidth, 46})) session.menu = true;
    ImGui::SameLine();
    if (ImGui::Button("OPTIONS", {buttonWidth, 46})) session.settings = true;
    ImGui::PopFont();
    ImGui::EndDisabled();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void drawShop(bj::Game& game, Commands& commands, const Interface& ui) {
    const auto state = game.state();
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled({0, 0}, display, IM_COL32(8, 9, 18, 190));
    const float width = std::min(900.0f, display.x - 64);
    const float height = std::min(760.0f, display.y - 48);
    beginPanel("##shop", {(display.x - width) * .5f, (display.y - height) * .5f}, {width, height});
    const ImVec2 header = ImGui::GetCursorScreenPos();
    ImGui::PushFont(ui.heading);
    ImGui::TextColored(gold, "RELIC SHOP");
    ImGui::PopFont();
    const std::string coins = std::to_string(state.coins);
    const float numberWidth = ui.number->CalcTextSizeA(46, 1000, 0, coins.c_str()).x;
    const float labelWidth = ui.small->CalcTextSizeA(18, 1000, 0, "COINS").x;
    const float walletX = ImGui::GetWindowPos().x + width - 20 - labelWidth - numberWidth - 72;
    auto* draw = ImGui::GetWindowDrawList();
    drawCoinIcon(draw, {walletX + 22, header.y + 25}, 21, ui);
    draw->AddText(ui.number, 46, {walletX + 54, header.y}, ImGui::ColorConvertFloat4ToU32(gold), coins.c_str());
    draw->AddText(ui.small, 18, {walletX + 66 + numberWidth, header.y + 19},
        ImGui::ColorConvertFloat4ToU32(gold), "COINS");
    ImGui::SetCursorScreenPos({header.x, header.y + 56});
    ImGui::Separator();
    ImGui::Text("%d of 4 relic slots used", static_cast<int>(state.owned.size()));
    ImGui::PushFont(ui.small);
    ImGui::TextColored(muted, "Round cleared. Choose an upgrade or save your coins.");
    ImGui::PopFont();
    const float offerY = ImGui::GetCursorPosY() + 4;
    const float cardWidth = (width - 54) * .5f;
    const float offerHeight = height < 700 ? 216.0f : 240.0f;
    for (std::size_t i = 0; i < state.offers.size(); ++i) {
        const auto& item = relicInfo(state.offers[i]);
        const auto accent = rarityColor(item.rarity);
        ImGui::PushID(static_cast<int>(item.id));
        ImGui::SetCursorPos({20 + static_cast<float>(i) * (cardWidth + 14), offerY});
        ImGui::PushStyleColor(ImGuiCol_Border, accent);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {.105f, .12f, .15f, 1});
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2);
        ImGui::BeginChild("offer", {cardWidth, offerHeight}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        ImGui::PushFont(ui.button);
        ImGui::TextColored(cream, "%s", item.name);
        ImGui::PopFont();
        rarityBadge(item.rarity, ui);
        ImGui::PushFont(ui.small);
        ImGui::TextWrapped("%s", item.description);
        ImGui::PopFont();
        ImGui::SetCursorPosY(offerHeight - 60);
        const bool owned = game.hasRelic(item.id);
        const bool purchased = game.boughtThisShop(item.id);
        const bool full = game.state().owned.size() >= 4;
        const bool affordable = game.state().coins >= item.cost;
        const std::string label = owned ? "EQUIPPED" : purchased ? "SOLD THIS VISIT" : full ? "4 / 4 EQUIPPED" :
            !affordable ? "NEED " + std::to_string(item.cost) + " COINS" :
            "BUY / " + std::to_string(item.cost) + " COINS";
        ImGui::PushFont(ui.button);
        ImGui::BeginDisabled(commands.blocked || commands.used || owned || purchased || full || !affordable);
        if (ImGui::Button(label.c_str(), {-1, 44})) commands.apply([&]() { return game.buy(item.id); });
        ImGui::EndDisabled();
        ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }
    ImGui::SetCursorPos({20, offerY + offerHeight + 13});
    ImGui::Separator();
    ImGui::TextUnformatted("YOUR RELICS");
    ImGui::PushFont(ui.small);
    ImGui::TextColored(muted, "Sell for half the purchase price. A sale frees one slot.");
    ImGui::PopFont();
    const float inventoryY = ImGui::GetCursorPosY();
    if (state.owned.empty()) ImGui::TextDisabled("No relics equipped yet.");
    for (std::size_t i = 0; i < state.owned.size(); ++i) {
        const auto& item = relicInfo(state.owned[i]);
        ImGui::PushID(100 + static_cast<int>(item.id));
        ImGui::SetCursorPos({20, inventoryY + static_cast<float>(i) * 38});
        ImGui::TextColored(rarityColor(item.rarity), "%s", item.name);
        if (ImGui::IsItemHovered()) relicTooltip(item, ui);
        ImGui::SameLine(300);
        rarityBadge(item.rarity, ui);
        ImGui::SameLine(width - 190);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {12, 3});
        ImGui::BeginDisabled(commands.blocked || commands.used);
        const std::string label = "SELL / " + std::to_string(item.cost / 2);
        if (ImGui::Button(label.c_str(), {150, 31})) commands.apply([&]() { return game.sell(item.id); });
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
        ImGui::PopID();
    }
    ImGui::SetCursorPos({20, height - 64});
    ImGui::PushFont(ui.button);
    ImGui::BeginDisabled(commands.blocked || commands.used);
    if (ImGui::Button("NEXT ROUND [SPACE]", {330, 46})) commands.apply([&]() { return game.leaveShop(); });
    ImGui::EndDisabled();
    ImGui::PopFont();
    ImGui::End();
}

void drawGameOver(bj::Game& game, Session& session, const Interface& ui, GLFWwindow* window) {
    const auto state = game.state();
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled({0, 0}, display, IM_COL32(85, 8, 17, 170));
    beginPanel("##end", {display.x * .5f - 310, display.y * .5f - 230}, {620, 460}, true);
    ImGui::PushFont(ui.number);
    ImGui::SetWindowFontScale(1.55f);
    const float titleWidth = ImGui::CalcTextSize("GAME OVER").x;
    ImGui::SetCursorPosX((620 - titleWidth) * .5f);
    ImGui::TextColored(cream, "GAME OVER");
    ImGui::SetWindowFontScale(1);
    ImGui::PopFont();
    ImGui::SetCursorPos({45, 100});
    ImGui::Text("Round %d ended at %d / %d points.", state.round, state.roundScore, state.goal);
    ImGui::SetCursorPos({45, 133});
    ImGui::TextColored(bj::evaluateHand(state.player).bust ? red : cream, "Your hand: %d%s   /   Dealer: %d",
        bj::evaluateHand(state.player).total, bj::evaluateHand(state.player).bust ? " (BUST)" : "",
        bj::evaluateHand(state.dealer).total);
    ImGui::SetCursorPos({45, 170});
    ImGui::TextColored(gold, "Run score: %d    Hand wins: %d", state.totalScore, state.wins);
    ImGui::PushFont(ui.small);
    ImGui::SetCursorPos({45, 202});
    ImGui::TextColored(muted, "Seed %u", state.seed);
    ImGui::PopFont();
    ImGui::PushFont(ui.button);
    ImGui::PushStyleColor(ImGuiCol_Button, {.23f, .24f, .26f, 1});
    ImGui::BeginDisabled(session.settings || session.rules || session.transitioning());
    ImGui::SetCursorPos({145, 244});
    if (ImGui::Button("RETRY [SPACE]", {330, 50})) session.start(game);
    ImGui::SetCursorPos({145, 306});
    if (ImGui::Button("MAIN MENU", {330, 50})) session.menu = true;
    ImGui::SetCursorPos({145, 368});
    if (ImGui::Button("QUIT GAME", {330, 50})) glfwSetWindowShouldClose(window, GLFW_TRUE);
    ImGui::EndDisabled();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::End();
}

void centeredText(ImDrawList* draw, ImFont* font, float size, float centerX, float y, ImU32 color,
                  const std::string& text) {
    const float width = font->CalcTextSizeA(size, 10000, 0, text.c_str()).x;
    draw->AddText(font, size, {centerX - width * .5f, y}, color, text.c_str());
}

void drawHandValue(const bj::Snapshot& state, const HandFeedback& feedback, const TableLayout& layout,
                   const Interface& ui) {
    const ImVec2 origin{layout.contentX + layout.contentWidth - 210, layout.actionY - 38};
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    beginPanel("##hand-value", origin, {210, 184}, true, ImGuiWindowFlags_NoInputs);
    auto* draw = ImGui::GetWindowDrawList();
    const float centerX = origin.x + 105;
    const auto hand = bj::evaluateHand(state.player);
    if (feedback.remaining > 0) {
        const float alpha = std::min(1.0f, feedback.remaining / .25f);
        const std::string change = (feedback.delta >= 0 ? "+" : "") + std::to_string(feedback.delta);
        const float textWidth = ui.menuButton->CalcTextSizeA(34, 1000, 0, change.c_str()).x;
        const float left = centerX - (textWidth + 40) * .5f;
        const auto ink = ImGui::ColorConvertFloat4ToU32({cream.x, cream.y, cream.z, alpha});
        const bool redSuit = feedback.card.suit == bj::Suit::Hearts || feedback.card.suit == bj::Suit::Diamonds;
        const ImVec4 suit = redSuit ? red : cream;
        draw->AddText(ui.menuButton, 34, {left, origin.y}, ink, change.c_str());
        presentation::drawCardSuit(draw, feedback.card.suit, {left + textWidth + 24, origin.y + 18}, 32,
            ImGui::ColorConvertFloat4ToU32({suit.x, suit.y, suit.z, alpha}));
    }
    const char* label = hand.bust ? "YOUR HAND / BUST" : hand.soft ? "YOUR HAND / SOFT" :
        state.phase == bj::Phase::Ready && !state.player.empty() ? "LAST HAND" : "YOUR HAND";
    centeredText(draw, ui.small, 18, centerX, origin.y + 60, ImGui::ColorConvertFloat4ToU32(muted), label);
    centeredText(draw, ui.largeNumber, 104, centerX, origin.y + 78,
        ImGui::ColorConvertFloat4ToU32(hand.bust ? red : cream),
        state.player.empty() ? "--" : std::to_string(hand.total));
    ImGui::End();
    ImGui::PopStyleVar();
}

void drawTable(bj::Game& game, Commands& commands, Session& session, HandFeedback& feedback,
               const Interface& ui, GLFWwindow* window) {
    const auto state = game.state();
    const auto display = ImGui::GetIO().DisplaySize;
    const auto layout = tableLayout(display);
    if (state.phase == bj::Phase::RunLost) {
        drawGameOver(game, session, ui, window);
        return;
    }
    if (state.phase == bj::Phase::Shop) {
        drawShop(game, commands, ui);
        return;
    }
    drawSidebar(state, layout, session, commands, ui, display.y);
    drawRelicStrip(game, layout, ui);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    beginPanel("##actions", {controlsLeft(layout), layout.actionY + 42}, {460, 104}, true);
    ImGui::PushFont(ui.menuButton);
    ImGui::BeginDisabled(commands.blocked || commands.used);
    if (state.phase == bj::Phase::PlayerTurn) {
        ImGui::PushStyleColor(ImGuiCol_Button, {.08f, .35f, .30f, 1});
        if (ImGui::Button("HIT", {190, 78})) commands.apply([&]() { return game.hit(); });
        ImGui::PopStyleColor();
        ImGui::SameLine(206);
        ImGui::PushStyleColor(ImGuiCol_Button, {.43f, .22f, .10f, 1});
        if (ImGui::Button("STAND", {250, 78})) commands.apply([&]() { return game.stand(); });
        ImGui::PopStyleColor();
    } else if (state.phase == bj::Phase::Ready) {
        ImGui::SetCursorPosX(68);
        if (ImGui::Button("DEAL HAND", {320, 78})) commands.apply([&]() { return game.deal(); });
    } else if (state.phase == bj::Phase::RoundOver) {
        ImGui::SetCursorPosX(68);
        if (ImGui::Button(state.roundScore >= state.goal ? "SHOP" : "CONTINUE", {320, 78}))
            commands.apply([&]() { return game.advance(); });
    }
    ImGui::EndDisabled();
    ImGui::PopFont();
    ImGui::PushFont(ui.small);
    ImGui::SetCursorPos({state.phase == bj::Phase::PlayerTurn ? 0.0f : 68.0f, 84});
    ImGui::TextColored(muted, "%s", state.phase == bj::Phase::PlayerTurn ? "H" : "SPACE");
    if (state.phase == bj::Phase::PlayerTurn) {
        ImGui::SameLine(206);
        ImGui::TextColored(muted, "SPACE / S");
    }
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar();

    const auto& current = game.state();
    feedback.observe(current, 0, !session.menu);
    if (current.phase == bj::Phase::RunLost) {
        drawGameOver(game, session, ui, window);
        return;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    beginPanel("##dealer", {layout.contentX + 18, layout.boardY + 8}, {250, 102}, true, ImGuiWindowFlags_NoInputs);
    ImGui::PushFont(ui.small);
    ImGui::TextColored(muted, "DEALER");
    ImGui::PopFont();
    const auto dealer = bj::evaluateHand(current.dealer);
    const std::string dealerText = current.dealer.empty() ? "--" : current.phase == bj::Phase::PlayerTurn
        ? std::to_string(bj::evaluateHand({current.dealer.front()}).total) + " + ?" : std::to_string(dealer.total);
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 dealerOrigin = ImGui::GetWindowPos();
    draw->AddText(ui.largeNumber, 64, {dealerOrigin.x, dealerOrigin.y + 21},
        ImGui::ColorConvertFloat4ToU32(current.phase != bj::Phase::PlayerTurn && dealer.bust ? red : cream), dealerText.c_str());
    ImGui::End();

    beginPanel("##hand-reward", {layout.contentX + 18, layout.actionY - 34}, {layout.contentWidth - 250, 74}, true,
        ImGuiWindowFlags_NoInputs);
    if (current.phase == bj::Phase::RoundOver) {
        const auto hand = bj::evaluateHand(current.player);
        const ImVec4 color = current.outcome == bj::Outcome::Win ? green : current.outcome == bj::Outcome::Loss ? red : gold;
        ImGui::PushFont(ui.small);
        ImGui::TextColored(color, "%s / +%d COINS", current.outcome == bj::Outcome::Win
            ? (hand.blackjack ? "BLACKJACK" : "YOU WIN") : current.outcome == bj::Outcome::Loss
            ? (hand.bust ? "BUST" : "HOUSE WINS") : "PUSH", current.score.coins);
        ImGui::PopFont();
        ImGui::SetCursorPosY(23);
        ImGui::PushFont(ui.number);
        ImGui::TextColored(color, "+%d POINTS", current.score.total);
        ImGui::PopFont();
    } else if (current.phase == bj::Phase::Ready) {
        ImGui::SetCursorPosY(26);
        ImGui::PushFont(ui.button);
        ImGui::TextColored(muted, "READY TO DEAL");
        ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    drawHandValue(current, feedback, layout, ui);
}

void drawRules(Session& session, const Interface& ui) {
    if (!session.rules) return;
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x * .5f - 310, display.y * .5f - 270});
    ImGui::SetNextWindowSize({620, 540});
    ImGui::SetNextWindowFocus();
    if (ImGui::Begin("HOW TO PLAY", &session.rules, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
        heading("THE FOOL'S TABLE", ui);
        ImGui::TextWrapped("Each round gives you five hands to reach the points target.");
        ImGui::BulletText("Beat the dealer without going over 21.");
        ImGui::BulletText("Aces count as 1 or 11. Dealer stands on 17.");
        ImGui::TextWrapped("A win earns the hand total in points and 3 coins. Natural blackjack is 21 points; only relics add bonus points.");
        ImGui::TextWrapped("A loss or push earns no points and uses a hand. Reach the target to shop. Run out of hands below the target and the run ends.");
        ImGui::TextWrapped("Equip up to 4 relics. Hover one to read its effect. Shop sales return half its price.");
        if (ImGui::Button("GOT IT", {160, 44})) session.rules = false;
    }
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
    const auto display = ImGui::GetIO().DisplaySize;
    const auto layout = tableLayout(display);
    scene.tableViewport = {layout.contentX / display.x, layout.boardY / display.y,
        layout.contentWidth / display.x, layout.boardHeight / display.y};
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
        HandFeedback feedback;
        session.fixedSeed = options.fixedSeed || options.smoke;
        session.transitions = !options.noAnimation;
        if (options.demo) {
            session.menu = false;
            session.started = true;
            game.deal();
        }
        int frame = 0;
        int fullShopResizeFrame = -1;
        double previousTime = glfwGetTime();
        std::set<std::string> captured;
        while (!glfwWindowShouldClose(window.handle)) {
            glfwPollEvents();
            if (options.smoke) {
                if (frame == 0 || frame == 100) glfwSetWindowSize(window.handle, 1100, 720);
                if (frame == 8 || frame == 160) glfwSetWindowSize(window.handle, 1440, 900);
                if (fullShopResizeFrame < 0 && game.state().phase == bj::Phase::Shop && game.state().owned.size() == 4)
                    fullShopResizeFrame = frame;
                if (frame == fullShopResizeFrame && fullShopResizeFrame >= 0)
                    glfwSetWindowSize(window.handle, 1100, 720);
                if (fullShopResizeFrame >= 0 && frame == fullShopResizeFrame + 20)
                    glfwSetWindowSize(window.handle, 1440, 900);
            }
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
                    const auto layout = menuLayout(io.DisplaySize);
                    io.AddMousePosEvent(layout.origin.x + layout.width * 0.5f, layout.origin.y + layout.height * 0.5f);
                    io.AddMouseButtonEvent(0, frame == 10);
                }
                if (!options.demo && options.seed == 42 && !options.noAnimation) {
                    if (frame == 67 || frame == 68) {
                        const auto center = dealButtonCenter(tableLayout(io.DisplaySize));
                        io.AddMousePosEvent(center.x, center.y);
                        io.AddMouseButtonEvent(0, frame == 67);
                    }
                    if (frame == 70 || frame == 90) io.AddKeyEvent(ImGuiKey_Space, true);
                    if (frame == 71 || frame == 91) io.AddKeyEvent(ImGuiKey_Space, false);
                    if (frame == 73 || frame == 74) {
                        const auto layout = tableLayout(io.DisplaySize);
                        io.AddMousePosEvent(layout.sidebar * .5f + 8, io.DisplaySize.y - 106);
                        io.AddMouseButtonEvent(0, frame == 73);
                    }
                    if (frame == 76) io.AddKeyEvent(ImGuiKey_Escape, true);
                    if (frame == 77) io.AddKeyEvent(ImGuiKey_Escape, false);
                    if (frame == 83 || frame == 84) {
                        const auto layout = tableLayout(io.DisplaySize);
                        io.AddMousePosEvent(layout.sidebar - 90, io.DisplaySize.y - 58);
                        io.AddMouseButtonEvent(0, frame == 83);
                    }
                    if (frame == 86) io.AddKeyEvent(ImGuiKey_F1, true);
                    if (frame == 87) io.AddKeyEvent(ImGuiKey_F1, false);
                }
            }
            ImGui::NewFrame();
            Commands commands{session.menu || session.settings || session.rules || session.transitioning()};
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                if (session.rules) session.rules = false;
                else if (session.settings) session.settings = false;
                else if (!session.transitioning()) session.menu = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F1, false) && !session.transitioning()) {
                session.rules = false;
                session.settings = !session.settings;
            }
            commands.blocked = session.menu || session.settings || session.rules || session.transitioning();
            if (!ImGui::GetIO().WantTextInput && !session.settings && !session.rules && !session.transitioning()) {
                if (ImGui::IsKeyPressed(ImGuiKey_H, false)) commands.apply([&]() { return game.hit(); });
                if (ImGui::IsKeyPressed(ImGuiKey_S, false)) commands.apply([&]() { return game.stand(); });
                // Space belongs to the primary game action, including when a button has focus.
                // Enter remains available for activating a focused UI control.
                if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && !commands.used) {
                    if (session.menu || game.state().phase == bj::Phase::RunLost) {
                        session.start(game);
                        commands.used = true;
                    }
                    else primaryAction(game, commands);
                }
            }
            // The smoke path traverses the real menu/fade and the game commands.
            if (options.autoplay && frame > 0 && frame % 10 == 0 && !session.transitioning() && !session.settings && !session.rules) {
                if ((session.menu && !options.smoke) || game.state().phase == bj::Phase::RunLost) session.start(game);
                else autoplayStep(game, commands);
                std::cout << "Frame " << frame << ": round=" << game.state().round
                    << " hands=" << game.state().handsLeft
                    << " points=" << game.state().roundScore << '/' << game.state().goal
                    << " phase=" << static_cast<int>(game.state().phase)
                    << " coins=" << game.state().coins << " seed=" << game.state().seed << '\n';
            }
            feedback.observe(game.state(), dt, !session.menu && !session.transitioning());
            if (session.menu) drawMenu(game, session, interface, window.handle);
            else drawTable(game, commands, session, feedback, interface, window.handle);
            drawSettings(session, settings, interface);
            drawRules(session, interface);
            feedback.observe(game.state(), 0, !session.menu && !session.transitioning());
            if (options.smoke && ((frame == 2 && !session.settings) || (frame == 8 && session.settings) ||
                (!options.demo && frame == 12 && (options.noAnimation ? session.menu : !session.transitioning()))))
                throw std::runtime_error("Smoke input failed: Settings/Start did not respond.");
            if (options.smoke && !options.demo && options.seed == 42 && !options.noAnimation &&
                ((frame == 69 && (game.state().handsLeft != 4 || game.state().phase != bj::Phase::PlayerTurn)) ||
                 (frame == 71 && game.state().phase != bj::Phase::RoundOver) ||
                 (frame == 75 && !session.rules) || (frame == 78 && session.rules) ||
                 (frame == 85 && !session.settings) || (frame == 88 && session.settings) ||
                 (frame == 91 && (game.state().handsLeft != 3 || session.settings || session.rules || session.menu))))
                throw std::runtime_error("Smoke input failed: Space did not deal/stand after a mouse click.");
            const float sceneTime = options.hidden && options.frames > 0 ? frame / 60.0f : static_cast<float>(now);
            renderer.render(makeScene(game.state(), sceneTime, session.menu), settings, width, height);
            if (session.transitioning()) ImGui::GetForegroundDrawList()->AddRectFilled(
                {0, 0}, ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, static_cast<int>(255 * session.darkness())));
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            const GLenum error = glGetError();
            if (error != GL_NO_ERROR) throw std::runtime_error("OpenGL error: " + std::to_string(error));
            if (!options.captures.empty() && !session.transitioning() && !commands.used) {
                const std::string name = session.rules ? "rules" : session.settings ? "settings" : session.menu ? "menu" :
                    game.state().phase == bj::Phase::Shop ? (game.state().owned.size() == 4 ? "shop-full" : "shop") :
                    game.state().phase == bj::Phase::RunLost ? "end" :
                    game.state().phase == bj::Phase::RoundOver && game.state().roundScore >= game.state().goal ? "round-clear" :
                    game.state().phase == bj::Phase::RoundOver && game.state().outcome == bj::Outcome::Loss &&
                        game.state().handsLeft > 0 ? "hand-loss" : "table";
                const auto capture = [&](const std::string& tag) {
                    if (captured.insert(tag).second) saveScreenshot(options.captures / (tag + ".png"), width, height);
                };
                capture(name);
                const auto hand = bj::evaluateHand(game.state().player);
                if (feedback.remaining > 0 && !session.menu && !session.settings && !session.rules &&
                    game.state().phase != bj::Phase::RunLost) {
                    capture("card-gain");
                    if (feedback.delta <= 0) capture("ace-recount");
                }
                if (hand.bust && game.state().phase == bj::Phase::RoundOver) capture("bust");
                if (hand.blackjack && game.state().outcome == bj::Outcome::Win &&
                    game.state().phase == bj::Phase::RoundOver) {
                    capture("blackjack");
                    if (game.hasRelic(bj::Relic::BlackjackBonus)) capture("blackjack-bonus");
                }
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
