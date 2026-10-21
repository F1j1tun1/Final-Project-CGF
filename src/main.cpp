#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "game/Game.hpp"
#include "graphics/Renderer.hpp"
#include "presentation/CardSuit.hpp"
#include "presentation/Presentation.hpp"
#include <iomanip>
#include <sstream>

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
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

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
    bj::StartDeck deck = bj::StartDeck::ExtraHand;
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
        } else if(arg=="--deck") {
            const auto deck=value();
            if(deck=="hands") options.deck=bj::StartDeck::ExtraHand;
            else if(deck=="life") options.deck=bj::StartDeck::SecondLife;
            else if(deck=="coins") options.deck=bj::StartDeck::Rich;
            else throw std::runtime_error("Deck must be hands, life or coins.");
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
            options.frames = 1200;
        } else if (arg == "--help") {
            std::cout << "Jester Jack\n"
                "  --seed N          Reproducible run (otherwise a fresh random seed)\n"
                "  --demo            Skip menu and deal the first hand\n"
                "  --deck NAME       Select hands, life or coins deck\n"
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
#ifdef _WIN32
        const auto native = glfwGetWin32Window(handle);
        const auto module = GetModuleHandleW(nullptr);
        SendMessageW(native, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(LoadIconW(module, MAKEINTRESOURCEW(101))));
        SendMessageW(native, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(LoadImageW(module, MAKEINTRESOURCEW(101), IMAGE_ICON, 32, 32, LR_SHARED)));
#endif
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
    bool collection = false;
    bool deckSelection = false;
    int collectionFilter = 0;
    bj::Relic selectedRelic = bj::Relic::RoyalPurse;
    bj::StartDeck selectedDeck = bj::StartDeck::ExtraHand;
    float animationSpeed = 0.0f;
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
        collection = false;
        deckSelection = false;
        rules = false;
        switched = false;
        fade = 0.0f;
    }
    void update(float dt, bj::Game& game) {
        if (!transitioning()) return;
        fade += (transitions ? dt : 1.0f) / 0.9f;
        if (fade >= 0.5f && !switched) {
            game.restart(nextSeed, selectedDeck);
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
    bj::Game* game = nullptr;
    presentation::Presentation* timeline = nullptr;
    template <typename Action> void apply(Action action) {
        if (blocked || used) return;
        const auto before = game->state();
        if (!action()) return;
        used = true;
        const auto& after = game->state();
        auto kind = presentation::Action::Other;
        if (before.phase == bj::Phase::Ready && after.handsLeft < before.handsLeft)
            kind = presentation::Action::Deal;
        else if (before.phase == bj::Phase::PlayerTurn)
            kind = after.player.size() > before.player.size() ||
                before.usedSecondChance != after.usedSecondChance ? presentation::Action::Hit : presentation::Action::Stand;
        timeline->begin(before, after, kind);
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
        const bool appended = state.player.size() == previous.size() + 1 && previous.size() >= 2 &&
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
    float height = 76;
    float step = 88;
};

MenuLayout menuLayout(ImVec2 display) {
    return {{display.x * 0.075f, display.y * 0.39f}, 350, display.y < 800 ? 64.0f : 76.0f, display.y < 800 ? 76.0f : 88.0f};
}

void drawMenu(bj::Game&, Session& session, const Interface& ui, GLFWwindow* window) {
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

    if(session.collection || session.deckSelection) return;

    const auto layout = menuLayout(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    beginPanel("##menu", layout.origin, {layout.width, 4 * layout.step}, true);
    ImGui::BeginDisabled(session.transitioning() || session.settings || session.collection || session.deckSelection);
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
        session.deckSelection = true;
    ImGui::SetCursorPos({0, layout.step});
    if (button("OPTIONS", {.29f, .31f, .34f, 1}, {.40f, .43f, .46f, 1}, {.22f, .24f, .27f, 1}))
        session.settings = true;
    ImGui::SetCursorPos({0, 2 * layout.step});
    if (button("COLLECTION", {.30f, .15f, .45f, 1}, {.44f, .23f, .60f, 1}, {.23f, .10f, .35f, 1}))
        session.collection = true;
    ImGui::SetCursorPos({0, 3 * layout.step});
    if (button("EXIT", {.56f, .16f, .20f, 1}, {.70f, .23f, .27f, 1}, {.44f, .11f, .14f, 1}))
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    ImGui::PopFont();
    ImGui::EndDisabled();
    ImGui::End();
    ImGui::PopStyleVar();
    backdrop->AddText({layout.origin.x, display.y - 49}, IM_COL32(234, 213, 190, 225),
        "Choose your deck. Challenge the house.");
}

ImVec4 rarityColor(bj::Rarity rarity) {
    switch (rarity) {
        case bj::Rarity::Common: return {0.77f, 0.81f, 0.79f, 1};
        case bj::Rarity::Rare: return {0.32f, 0.67f, 1.0f, 1};
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
    switch(relic) {
        case bj::Relic::Precision: return "21: +0.5 mult / +1 coin";
        case bj::Relic::LongHand: return "4+ cards: +0.6 mult / +1 coin";
        case bj::Relic::SecondChance: return "Bust rescue / Win: +1 mult";
        case bj::Relic::Streak: return "Win streak: up to +0.4 mult";
        case bj::Relic::RoyalPurse: return "Blackjack: +1 coin";
        case bj::Relic::SoftTouch: return "Soft win: +1 coin";
        case bj::Relic::NarrowEscape: return "House bust: +0.4 mult, +1 coin";
        case bj::Relic::RedRibbon: return "Red opening pair: +1 coin";
        case bj::Relic::BlackVelvet: return "Black opening pair: +1 coin";
        case bj::Relic::EvenOdds: return "Even win: +1 coin";
        case bj::Relic::CourtFavor: return "J / Q / K win: +3 points";
        case bj::Relic::HumbleCrown: return "Win at 18 or less: +5 points";
        case bj::Relic::BlackjackBonus: return "Blackjack: +5 points";
        case bj::Relic::CopperBell: return "3+ cards: +3 points";
        case bj::Relic::SeventeenCharm: return "Win at 17: +1 coin";
        case bj::Relic::ScarletMask: return "All red: +0.5 mult";
        case bj::Relic::MidnightMask: return "All black: +0.5 mult";
        case bj::Relic::RoyalCourt: return "2+ face cards: +0.75 mult";
        case bj::Relic::SoftCrown: return "Soft 19+: +0.75 mult";
        case bj::Relic::JesterCrown: return "Blackjack: +1 mult";
        case bj::Relic::FiveCardParade: return "5+ cards: +1.5 mult";
        case bj::Relic::FinalAct: return "Last hand: +1 mult";
        case bj::Relic::Underdog: return "Low win / house bust: +1.5 mult";
        case bj::Relic::Encore: return "3+ earlier wins: +1 mult";
    }
    return "";
}

void drawChip(ImDrawList* draw, ImVec2 c, float r, bj::Rarity rarity, int symbol, const Interface& ui) {
    const auto color = ImGui::ColorConvertFloat4ToU32(rarityColor(rarity));
    draw->AddCircleFilled({c.x+2,c.y+3},r,IM_COL32(0,0,0,130),40);
    draw->AddCircleFilled(c,r,color,40);
    draw->AddCircleFilled(c,r*.76f,IM_COL32(28,24,42,255),40);
    draw->AddCircle(c,r*.64f,color,40,1.5f);
    for(int i=0;i<8;++i) {
        const float a=i*6.2831853f/8;
        draw->AddLine({c.x+std::cos(a)*r*.80f,c.y+std::sin(a)*r*.80f},
            {c.x+std::cos(a)*r*.96f,c.y+std::sin(a)*r*.96f},IM_COL32(255,239,202,240),r*.13f);
    }
    const char* marks[] = {"J","A","K","Q","7","*"};
    const auto text=marks[symbol%6];
    const float size=r*1.05f;
    const auto extent=ui.button->CalcTextSizeA(size,1000,0,text);
    draw->AddText(ui.button,size,{c.x-extent.x*.5f,c.y-extent.y*.5f},color,text);
}

void drawHandIcon(ImDrawList* draw, ImVec2 c, float r) {
    const auto color=ImGui::ColorConvertFloat4ToU32(blue);
    draw->AddRectFilled({c.x-r*.6f,c.y-r*.1f},{c.x+r*.55f,c.y+r*.7f},color,r*.25f);
    for(int i=0;i<4;++i) {
        const float x=c.x-r*.5f+i*r*.29f;
        const float top=c.y-r*(i==1?1.05f:i==0?.75f:i==2?.92f:.6f);
        draw->AddRectFilled({x,top},{x+r*.21f,c.y+r*.2f},color,r*.11f);
    }
    draw->AddLine({c.x-r*.45f,c.y+r*.35f},{c.x-r*.98f,c.y-r*.08f},color,r*.32f);
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
    return {sidebar, contentX, display.x - contentX - 20, 176, display.y - 176 - 110, actionY};
}

float controlsLeft(const TableLayout& layout) {
    constexpr float width = 456;
    const float tableCenter = layout.contentX + layout.contentWidth * .5f;
    const float handCenter = layout.contentX + layout.contentWidth - 105;
    // Leave room for two large hand digits at the minimum window width.
    const float center = std::min(tableCenter, handCenter - 80 - width * .5f);
    return center - width * .5f;
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
    beginPanel("##relics", {layout.contentX, 16}, {layout.contentWidth, 154}, true);
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
        ImGui::BeginChild("slot", {width, 120}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        if (item) {
            const ImVec2 namePosition = ImGui::GetCursorScreenPos();
            const float available = ImGui::GetContentRegionAvail().x - 35;
            const float nameWidth = ui.button->CalcTextSizeA(26, 1000, 0, item->name).x;
            const float nameSize = 26 * std::min(1.0f, available / nameWidth);
            drawChip(ImGui::GetWindowDrawList(), {namePosition.x+available+19,namePosition.y+15}, 17, item->rarity, static_cast<int>(item->id), ui);
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
        const auto pos=ImGui::GetWindowPos();
        const ImVec2 center{pos.x+half-32,pos.y+56};
        if(column) drawCoinIcon(ImGui::GetWindowDrawList(),center,18,ui);
        else drawHandIcon(ImGui::GetWindowDrawList(),center,18);
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
    if (state.startDeck == bj::StartDeck::SecondLife) ImGui::TextColored(gold, "REVIVE: %d / 1", state.revivesLeft);
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

void drawGameOver(bj::Game& game, Commands& commands, Session& session, const Interface& ui, GLFWwindow* window) {
    const auto state = game.state();
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled({0, 0}, display, IM_COL32(85, 8, 17, 170));
    beginPanel("##end", {display.x * .5f - 310, display.y * .5f - 265}, {620, 530}, true);
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
    if (state.revivesLeft > 0) {
        if (ImGui::Button("REVIVE / ONE LIFE", {330, 50})) commands.apply([&]() {return game.revive();});
    } else if (ImGui::Button("RETRY [SPACE]", {330, 50})) session.start(game);
    if(state.revivesLeft > 0) {
        ImGui::SetCursorPos({145,306});
        if(ImGui::Button("NEW RUN",{330,50})) session.start(game);
    }
    ImGui::SetCursorPos({145, state.revivesLeft > 0 ? 368.0f : 306.0f});
    if (ImGui::Button("MAIN MENU", {330, 50})) session.menu = true;
    ImGui::SetCursorPos({145, state.revivesLeft > 0 ? 430.0f : 368.0f});
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

bj::Snapshot settledState(const presentation::Presentation& timeline) {
    auto state=timeline.visible();
    if(timeline.playerArrival()<1 && !state.player.empty()) state.player.pop_back();
    if(timeline.dealerArrival()<1 && !state.dealer.empty()) state.dealer.pop_back();
    return state;
}

void drawTable(bj::Game& game, Commands& commands, Session& session, HandFeedback& feedback,
               const Interface& ui, GLFWwindow* window, const presentation::Presentation& timeline) {
    const auto state = settledState(timeline);
    const auto display = ImGui::GetIO().DisplaySize;
    const auto layout = tableLayout(display);
    if (state.phase == bj::Phase::RunLost && !timeline.busy()) {
        drawGameOver(game, commands, session, ui, window);
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

    const auto& current = state;
    feedback.observe(current, 0, !session.menu);
    if (current.phase == bj::Phase::RunLost && !timeline.busy()) {
        drawGameOver(game, commands, session, ui, window);
        return;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    beginPanel("##dealer", {layout.contentX + 18, layout.boardY + 8}, {250, 102}, true, ImGuiWindowFlags_NoInputs);
    ImGui::PushFont(ui.small);
    ImGui::TextColored(muted, "DEALER");
    ImGui::PopFont();
    const auto dealer = bj::evaluateHand(current.dealer);
    const std::string dealerText = current.dealer.empty() ? "--" : !timeline.dealerRevealed()
        ? std::to_string(bj::evaluateHand({current.dealer.front()}).total) + " + ?" : std::to_string(dealer.total);
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 dealerOrigin = ImGui::GetWindowPos();
    draw->AddText(ui.largeNumber, 64, {dealerOrigin.x, dealerOrigin.y + 21},
        ImGui::ColorConvertFloat4ToU32(current.phase != bj::Phase::PlayerTurn && dealer.bust ? red : cream), dealerText.c_str());
    ImGui::End();

    beginPanel("##hand-reward", {layout.contentX + 18, layout.actionY - 76}, {layout.contentWidth - 250, 112}, true,
        ImGuiWindowFlags_NoInputs);
    if (current.phase == bj::Phase::RoundOver || timeline.rewardStep() >= 0) {
        const auto hand = bj::evaluateHand(current.player);
        const ImVec4 color = current.outcome == bj::Outcome::Win ? green : current.outcome == bj::Outcome::Loss ? red : gold;
        ImGui::PushFont(ui.small);
        ImGui::TextColored(color, "%s / +%d COINS", current.outcome == bj::Outcome::Win
            ? (hand.blackjack ? "BLACKJACK" : "YOU WIN") : current.outcome == bj::Outcome::Loss
            ? (hand.bust ? "BUST" : "HOUSE WINS") : "PUSH", current.score.coins);
        ImGui::PopFont();
        ImGui::SetCursorPosY(23);
        ImGui::PushFont(ui.number);
        const int step=timeline.rewardStep();
        const int count=static_cast<int>(current.score.appliedEffects.size());
        if(step==0) ImGui::TextColored(color,"%d BASE POINTS",current.score.base);
        else if(step>0 && step<=count) {
            const auto& effect=current.score.appliedEffects[static_cast<std::size_t>(step-1)];
            const auto& item=relicInfo(effect.relic);
            ImGui::PopFont(); ImGui::PushFont(ui.button);
            ImGui::TextColored(rarityColor(item.rarity),"%s",item.name);
        } else ImGui::TextColored(color, "+%d POINTS", current.score.total);
        ImGui::PopFont();
    } else if (current.phase == bj::Phase::Ready) {
        ImGui::SetCursorPosY(26);
        ImGui::PushFont(ui.button);
        ImGui::TextColored(muted, "READY TO DEAL");
        ImGui::PopFont();
    }
    if (!current.score.appliedEffects.empty() && timeline.rewardStep()>0) {
        ImGui::PushFont(ui.small);
        ImGui::SetCursorPosY(72);
        const auto n=std::min(timeline.rewardVisibleEffects(),current.score.appliedEffects.size());
        std::string text;
        for(std::size_t i=0;i<n;++i) {
            const auto& effect=current.score.appliedEffects[i];
            if(!text.empty()) text+=" / ";
            text+=relicInfo(effect.relic).name;
            std::ostringstream bonus;
            if(effect.points) bonus<<" +"<<effect.points;
            if(effect.multiplierBonus>0) bonus<<" +"<<std::setprecision(2)<<effect.multiplierBonus<<" mult";
            if(effect.coins) bonus<<" +"<<effect.coins<<" coin";
            text+=bonus.str();
        }
        ImGui::TextWrapped("%s",text.c_str());
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
        ImGui::TextWrapped("Each round gives you five hands to reach the target. The Last Laugh deck adds one more.");
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
        ImGui::Spacing();
        ImGui::TextUnformatted("DEALING SPEED");
        ImGui::SliderFloat("##speed", &session.animationSpeed, 0.0f, 1.0f, "");
        ImGui::TextColored(muted,"Natural                         Fast");
        ImGui::TextWrapped("Click or press Space to speed up a deal or reward.");
        ImGui::Spacing();
        ImGui::TextWrapped("Camera: drag the free table with the left mouse button. Scroll to zoom. Double-click to reset.");
        if (ImGui::Button("RESET VIEW")) settings = gfx::RenderSettings{};
        ImGui::SameLine();
        if (ImGui::Button("DONE")) session.settings = false;
    }
    ImGui::End();
}

gfx::Scene makeScene(const presentation::Presentation& timeline, float time, bool menu) {
    const auto& state=timeline.visible();
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
            i == 1 && !timeline.dealerRevealed() && !timeline.dealerFlipping()});
        if(i==1 && timeline.dealerFlipping()) scene.dealer.back().flip=timeline.dealerFlip();
    }
    if(!scene.player.empty()) scene.player.back().arrival=timeline.playerArrival();
    if(!scene.dealer.empty()) scene.dealer.back().arrival=timeline.dealerArrival();
    scene.dealerReach=timeline.dealerReach();
    scene.dealingToPlayer=timeline.dealingToPlayer();
    scene.celebration=timeline.celebration();
    scene.legendary=timeline.legendPulse();
    return scene;
}

void drawCollection(Session& session, const Interface& ui) {
    if(!session.collection) return;
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},display,IM_COL32(12,7,24,200));
    const ImVec2 size{std::min(1180.0f,display.x-48),display.y-48};
    beginPanel("##collection",{(display.x-size.x)*.5f,24},size);
    heading("THE RELIC COLLECTION",ui);
    const char* filters[]={"ALL  /  24","COMMON  /  10","RARE  /  8","LEGENDARY  /  6"};
    for(int i=0;i<4;++i) {
        if(i) ImGui::SameLine();
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button,session.collectionFilter==i?ImVec4(.40f,.25f,.12f,1):ImVec4(.20f,.15f,.27f,1));
        if(ImGui::Button(filters[i],{160,36})) session.collectionFilter=i;
        ImGui::PopStyleColor(); ImGui::PopID();
    }
    ImGui::BeginChild("gallery",{size.x-340,size.y-180},false);
    const float w=(ImGui::GetContentRegionAvail().x-24)/3;
    int shown=0;
    for(const auto& item:bj::relicCatalog()) {
        if(session.collectionFilter && static_cast<int>(item.rarity)!=session.collectionFilter-1) continue;
        if(shown%3) ImGui::SameLine();
        ImGui::PushID(static_cast<int>(item.id));
        const auto pos=ImGui::GetCursorScreenPos();
        const bool selected=session.selectedRelic==item.id;
        const auto accent=rarityColor(item.rarity);
        ImGui::PushStyleColor(ImGuiCol_Header,selected?ImVec4(.27f,.19f,.33f,1):ImVec4(.12f,.12f,.18f,1));
        if(ImGui::Selectable("##relic",selected,0,{w,134})) session.selectedRelic=item.id;
        ImGui::PopStyleColor();
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRect(pos,{pos.x+w,pos.y+134},ImGui::ColorConvertFloat4ToU32(accent),8,0,selected?3.0f:1.0f);
        drawChip(draw,{pos.x+w*.5f,pos.y+40},28,item.rarity,static_cast<int>(item.id),ui);
        const float available=w-14;
        const float nameWidth=ui.button->CalcTextSizeA(24,1000,0,item.name).x;
        const float fontSize=24*std::min(1.0f,available/nameWidth);
        centeredText(draw,ui.button,fontSize,pos.x+w*.5f,pos.y+78,ImGui::ColorConvertFloat4ToU32(cream),item.name);
        centeredText(draw,ui.small,18,pos.x+w*.5f,pos.y+108,ImGui::ColorConvertFloat4ToU32(accent),bj::rarityName(item.rarity));
        ImGui::PopID(); ++shown;
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("detail",{280,size.y-180},true);
    const auto& item=relicInfo(session.selectedRelic);
    const auto pos=ImGui::GetCursorScreenPos();
    drawChip(ImGui::GetWindowDrawList(),{pos.x+115,pos.y+74},62,item.rarity,static_cast<int>(item.id),ui);
    ImGui::Dummy({230,155});
    ImGui::PushFont(ui.heading); ImGui::TextWrapped("%s",item.name); ImGui::PopFont();
    rarityBadge(item.rarity,ui);
    ImGui::Spacing(); ImGui::TextWrapped("%s",item.description);
    ImGui::Spacing(); ImGui::TextColored(gold,"%d COINS",item.cost);
    ImGui::PushFont(ui.small);
    ImGui::TextWrapped("Multipliers add together. Point bonuses are added before the multiplier. All 24 relics are available from the beginning.");
    ImGui::PopFont();
    ImGui::EndChild();
    if(ImGui::Button("BACK",{180,44})) session.collection=false;
    ImGui::End();
}

float deckPanelHeight(ImVec2 display) {return std::min(620.0f,display.y-56);}

void drawDeckSelection(bj::Game& game, Session& session, const Interface& ui) {
    if(!session.deckSelection) return;
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},display,IM_COL32(12,7,24,205));
    const float width=std::min(1000.0f,display.x-56),height=deckPanelHeight(display);
    beginPanel("##decks",{(display.x-width)*.5f,(display.y-height)*.5f},{width,height});
    heading("CHOOSE YOUR DECK",ui);
    ImGui::TextColored(muted,"One gift from the house. Make it count.");
    const bj::StartDeck decks[]={bj::StartDeck::ExtraHand,bj::StartDeck::SecondLife,bj::StartDeck::Rich};
    const char* names[]={"LAST LAUGH","ENCORE","GOLDEN FOOL"};
    const char* descriptions[]={"One extra hand in every round.\n6 hands instead of 5.","One revival for the entire run.\nReplay a failed round and keep your relics and coins.","Start the run with 10 extra coins.\nFive hands in every round."};
    const ImVec4 colors[]={blue,red,gold};
    const float w=(width-68)/3,y=120,h=height-220;
    for(int i=0;i<3;++i) {
        ImGui::PushID(i); ImGui::SetCursorPos({20+i*(w+14),y});
        const auto pos=ImGui::GetCursorScreenPos();
        const auto ink=ImGui::ColorConvertFloat4ToU32(colors[i]);
        const bool selected=session.selectedDeck==decks[i];
        ImGui::PushStyleColor(ImGuiCol_Header,selected?ImVec4(.27f,.20f,.30f,1):ImVec4(.10f,.12f,.17f,1));
        if(ImGui::Selectable("##deck",selected,0,{w,h})) session.selectedDeck=decks[i];
        ImGui::PopStyleColor();
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRect(pos,{pos.x+w,pos.y+h},ink,10,0,selected?3.0f:1.0f);
        const ImVec2 c{pos.x+w*.5f,pos.y+88};
        for(int card=0;card<3;++card) {
            const ImVec2 a{c.x-40+card*10,c.y-50+card*4};
            draw->AddRectFilled(a,{a.x+66,a.y+92},IM_COL32(37,24,52,255),7);
            draw->AddRect(a,{a.x+66,a.y+92},ink,7,0,2);
        }
        if(i==0) drawHandIcon(draw,{c.x+8,c.y+7},22);
        else if(i==2) drawCoinIcon(draw,{c.x+8,c.y+7},24,ui);
        else centeredText(draw,ui.number,46,c.x+8,c.y-15,ink,"+1");
        centeredText(draw,ui.button,26,pos.x+w*.5f,pos.y+158,ink,names[i]);
        draw->AddText(ui.small,18,{pos.x+18,pos.y+205},ImGui::ColorConvertFloat4ToU32(cream),descriptions[i],nullptr,w-36);
        ImGui::PopID();
    }
    ImGui::SetCursorPos({20,height-66});
    if(ImGui::Button("BACK",{180,46})) session.deckSelection=false;
    ImGui::SameLine();
    ImGui::PushFont(ui.button);
    if(ImGui::Button("TAKE A SEAT",{width-232,46})) session.start(game);
    ImGui::PopFont(); ImGui::End();
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
        bj::Game game(options.seed,options.demo ? options.deck : bj::StartDeck::Standard);
        Session session;
        HandFeedback feedback;
        presentation::Presentation timeline;
        timeline.reset(game.state());
        bool cameraDragging = false;
        bool smokeDealt=false, smokeStood=false, smokeCollection=false, smokeDeck=false, smokeRevived=false;
        int maxOwned=0,maxRound=1;
        session.selectedDeck=options.deck;
        session.fixedSeed = options.fixedSeed || options.smoke;
        session.transitions = !options.noAnimation;
        if (options.demo) {
            session.menu = false;
            session.started = true;
            const auto before=game.state();
            game.deal();
            timeline.begin(before,game.state(),presentation::Action::Deal);
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
            const bool switched=session.switched;
            session.update(dt, game);
            if(!switched && session.switched) timeline.reset(game.state());
            if(!session.menu && !session.settings && !session.rules) timeline.update(dt,session.animationSpeed);
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            if(options.smoke) {
                auto& io=ImGui::GetIO();
                if(frame==1 || frame==6) io.AddKeyEvent(ImGuiKey_F1,true);
                if(frame==2 || frame==7) io.AddKeyEvent(ImGuiKey_F1,false);
                const auto menu=menuLayout(io.DisplaySize);
                if(frame==10 || frame==11) {
                    io.AddMousePosEvent(menu.origin.x+menu.width*.5f,menu.origin.y+2*menu.step+menu.height*.5f);
                    io.AddMouseButtonEvent(0,frame==10);
                }
                if(frame==14) io.AddKeyEvent(ImGuiKey_Escape,true);
                if(frame==15) io.AddKeyEvent(ImGuiKey_Escape,false);
                if(frame==12 || frame==13) {
                    io.AddMousePosEvent(io.DisplaySize.x*.5f-86,120);
                    io.AddMouseButtonEvent(0,frame==12);
                }
                if(frame==18 || frame==19) {
                    io.AddMousePosEvent(menu.origin.x+menu.width*.5f,menu.origin.y+menu.height*.5f);
                    io.AddMouseButtonEvent(0,frame==18);
                }
                if(frame==22 || frame==23) {
                    const float panelHeight=deckPanelHeight(io.DisplaySize);
                    io.AddMousePosEvent(io.DisplaySize.x*.5f+100,(io.DisplaySize.y+panelHeight)*.5f-43);
                    io.AddMouseButtonEvent(0,frame==22);
                }
                if(frame==80 || frame==82 || frame==84 || frame==86) {
                    const auto table=tableLayout(io.DisplaySize);
                    io.AddMousePosEvent(table.contentX+table.contentWidth*.65f+(frame>=82?40:0),table.boardY+160);
                    if(frame==80) io.AddMouseButtonEvent(0,true);
                    if(frame==84) io.AddMouseButtonEvent(0,false);
                    if(frame==86) io.AddMouseWheelEvent(0,1);
                }
                if(frame>=90 && frame%20==0 && !session.menu && !timeline.busy() &&
                    game.state().phase!=bj::Phase::Shop && game.state().phase!=bj::Phase::RunLost)
                    io.AddKeyEvent(ImGuiKey_Space,true);
                if(frame>=91 && frame%20==1) io.AddKeyEvent(ImGuiKey_Space,false);
            }
            ImGui::NewFrame();
            const bool overlay=session.menu || session.settings || session.rules || session.collection || session.deckSelection || session.transitioning();
            const bool wasBusy=timeline.busy();
            Commands commands{overlay || wasBusy,false,&game,&timeline};
            if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)) {
                if(session.collection) session.collection=false;
                else if(session.deckSelection) session.deckSelection=false;
                else if(session.rules) session.rules=false;
                else if(session.settings) session.settings=false;
                else if(!session.transitioning()) session.menu=true;
            }
            if(ImGui::IsKeyPressed(ImGuiKey_F1,false) && !session.transitioning()) {
                session.rules=false; session.settings=!session.settings;
            }
            const bool space=ImGui::IsKeyPressed(ImGuiKey_Space,false);
            const bool gameSpace=ImGui::IsKeyDown(ImGuiKey_Space) && !session.settings && !session.rules &&
                !session.collection && !session.deckSelection;
            if(wasBusy && !overlay && (space || ImGui::IsMouseClicked(0))) timeline.accelerate();
            commands.blocked=session.menu || session.settings || session.rules || session.collection || session.deckSelection || session.transitioning() || wasBusy;
            if(!ImGui::GetIO().WantTextInput && !session.settings && !session.rules && !session.collection && !session.deckSelection && !session.transitioning() && !wasBusy) {
                if(ImGui::IsKeyPressed(ImGuiKey_H,false)) commands.apply([&](){return game.hit();});
                if(ImGui::IsKeyPressed(ImGuiKey_S,false)) commands.apply([&](){return game.stand();});
                if(space && !commands.used) {
                    if(session.menu) session.deckSelection=true;
                    else if(game.state().phase==bj::Phase::RunLost) {
                        if(game.state().revivesLeft>0) commands.apply([&](){return game.revive();});
                        else session.start(game);
                    } else primaryAction(game,commands);
                }
            }
            if(options.autoplay && frame>180 && frame%10==0 && !session.settings && !session.rules && !session.collection && !session.deckSelection && !session.transitioning()) {
                if(timeline.busy()) timeline.accelerate();
                else if(game.state().phase==bj::Phase::RunLost) {
                    if(game.state().revivesLeft) commands.apply([&](){return game.revive();});
                    else session.start(game);
                } else if(session.menu && !options.smoke) session.start(game);
                else autoplayStep(game,commands);
            }
            feedback.observe(settledState(timeline),dt,!session.menu && !session.transitioning());
            // Space is handled above; prevent focused buttons from receiving it again.
            ImGui::BeginDisabled(gameSpace);
            if(session.menu) drawMenu(game,session,interface,window.handle);
            else drawTable(game,commands,session,feedback,interface,window.handle,timeline);
            drawDeckSelection(game,session,interface);
            drawCollection(session,interface);
            drawSettings(session,settings,interface);
            drawRules(session,interface);
            ImGui::EndDisabled();
            const auto& io=ImGui::GetIO();
            const auto layout=tableLayout(io.DisplaySize);
            const bool freeTable=!session.menu && !session.settings && !session.rules && !session.transitioning() &&
                timeline.visible().phase!=bj::Phase::Shop && timeline.visible().phase!=bj::Phase::RunLost &&
                io.MousePos.x>layout.contentX && io.MousePos.y>layout.boardY && io.MousePos.y<layout.actionY &&
                !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
            if(!ImGui::IsMouseDown(0) || !freeTable) cameraDragging=false;
            if(freeTable && ImGui::IsMouseDoubleClicked(0)) {settings.yaw=0;settings.pitch=46;settings.distance=10.1f;cameraDragging=false;}
            else if(freeTable && ImGui::IsMouseClicked(0)) cameraDragging=true;
            if(cameraDragging && freeTable && ImGui::IsMouseDragging(0,3)) {
                settings.yaw=std::clamp(settings.yaw-io.MouseDelta.x*.20f,-35.0f,35.0f);
                settings.pitch=std::clamp(settings.pitch+io.MouseDelta.y*.16f,32.0f,68.0f);
            }
            if(freeTable && io.MouseWheel!=0) settings.distance=std::clamp(settings.distance-io.MouseWheel*.5f,8.0f,14.0f);
            if(options.smoke) {
                smokeCollection=smokeCollection||session.collection;
                smokeDeck=smokeDeck||session.deckSelection;
                smokeDealt=smokeDealt||game.state().handsLeft<game.state().maxHands;
                smokeStood=smokeStood||game.state().outcome!=bj::Outcome::None;
                smokeRevived=smokeRevived||game.state().revivalsUsed>0;
                maxOwned=std::max(maxOwned,static_cast<int>(game.state().owned.size()));
                maxRound=std::max(maxRound,game.state().round);
                if((frame==2 && !session.settings)||(frame==8 && session.settings)) throw std::runtime_error("Settings input failed");
                if(frame==85 && std::abs(settings.yaw)<1) throw std::runtime_error("Table drag did not rotate camera");
                if(frame==87 && settings.distance>=10.1f) throw std::runtime_error("Mouse wheel did not zoom camera");
            }
            const float sceneTime=options.hidden && options.frames>0 ? frame/60.0f : static_cast<float>(now);
            renderer.render(makeScene(timeline,sceneTime,session.menu),settings,width,height);
            if (session.transitioning()) ImGui::GetForegroundDrawList()->AddRectFilled(
                {0, 0}, ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, static_cast<int>(255 * session.darkness())));
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            const GLenum error = glGetError();
            if (error != GL_NO_ERROR) throw std::runtime_error("OpenGL error: " + std::to_string(error));
            if (!options.captures.empty() && !session.transitioning() && !commands.used) {
                const std::string name = session.collection ? "collection" : session.deckSelection ? "decks" : timeline.busy() ? (timeline.rewardStep()>=0 ? "reward" : "dealing") : session.rules ? "rules" : session.settings ? "settings" : session.menu ? "menu" :
                    game.state().phase == bj::Phase::Shop ? (game.state().owned.size() == 4 ? "shop-full" : "shop") :
                    game.state().phase == bj::Phase::RunLost ? "end" :
                    game.state().phase == bj::Phase::RoundOver && game.state().roundScore >= game.state().goal ? "round-clear" :
                    game.state().phase == bj::Phase::RoundOver && game.state().outcome == bj::Outcome::Loss &&
                        game.state().handsLeft > 0 ? "hand-loss" : "table";
                const auto capture = [&](const std::string& tag) {
                    if (captured.insert(tag).second) saveScreenshot(options.captures / (tag + ".png"), width, height);
                };
                capture(name);
                if(session.collection && session.collectionFilter) capture("collection-filter");
                const auto& visible=timeline.visible();
                const auto hand = bj::evaluateHand(visible.player);
                if(timeline.rewardStep()>0 && !visible.score.appliedEffects.empty())
                    capture("applied-relics-"+std::to_string(timeline.rewardStep()));
                if(visible.owned.size()==4 && visible.phase!=bj::Phase::Shop) capture("relics-full");
                if (feedback.remaining > 0 && !session.menu && !session.settings && !session.rules &&
                    game.state().phase != bj::Phase::RunLost) {
                    capture("card-gain");
                    if (feedback.delta <= 0) capture("ace-recount");
                }
                if (hand.bust && visible.phase == bj::Phase::RoundOver) capture("bust");
                if (!timeline.busy() && hand.blackjack && visible.outcome == bj::Outcome::Win &&
                    visible.phase == bj::Phase::RoundOver) {
                    capture("blackjack");
                    if (game.hasRelic(bj::Relic::BlackjackBonus)) capture("blackjack-bonus");
                }
            }
            if (options.smoke && !options.captures.empty() && (frame == 25 || frame == 38 || frame == 51))
                saveScreenshot(options.captures / ("fade-" + std::to_string(frame) + ".png"), width, height);
            if(options.smoke && !options.captures.empty() && frame==150)
                saveScreenshot(options.captures/"table-small.png",width,height);
            ++frame;
            const bool finished = options.frames > 0 && frame >= options.frames;
            if (finished && !options.screenshot.empty()) saveScreenshot(options.screenshot, width, height);
            glfwSwapBuffers(window.handle);
            if (finished) {
                if(options.smoke && !options.demo && frame>=400 && (!smokeCollection || !smokeDeck || !smokeDealt || !smokeStood))
                    throw std::runtime_error("Smoke traversal incomplete");
                if(options.smoke && !options.demo && options.deck==bj::StartDeck::SecondLife && frame>=2000 && !smokeRevived)
                    throw std::runtime_error("Revival traversal incomplete");
                break;
            }
        }
        std::cout << "Rendered frames: " << frame << '\n';
        if(options.smoke) std::cout << "Smoke: collection="<<smokeCollection<<" decks="<<smokeDeck
            <<" dealt="<<smokeDealt<<" resolved="<<smokeStood<<" revived="<<smokeRevived
            <<" max relics="<<maxOwned<<" max round="<<maxRound<<'\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Jester Jack: " << error.what() << '\n';
        return 1;
    }
}
