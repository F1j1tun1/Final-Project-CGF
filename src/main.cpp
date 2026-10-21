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
#include "graphics/ArtTexture.hpp"
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
constexpr ImVec4 gold{0.84f, 0.69f, 0.42f, 1.0f};
constexpr ImVec4 muted{0.68f, 0.64f, 0.61f, 1.0f};
constexpr ImVec4 cream{0.97f, 0.92f, 0.82f, 1.0f};
constexpr ImVec4 green{0.48f, 0.79f, 0.61f, 1.0f};
constexpr ImVec4 red{0.92f, 0.42f, 0.39f, 1.0f};
constexpr ImVec4 blue{0.39f, 0.66f, 0.88f, 1.0f};
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
    bool noAmbientMotion = false;
    int width = 1440;
    int height = 900;
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
        } else if (arg == "--size") {
            const auto size = value();
            const auto separator = size.find('x');
            if (separator == std::string::npos) throw std::runtime_error("Size must be WIDTHxHEIGHT.");
            const auto parseDimension = [](const std::string& text) {
                if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
                    throw std::runtime_error("Size must be WIDTHxHEIGHT.");
                return std::stoi(text);
            };
            options.width = parseDimension(size.substr(0, separator));
            options.height = parseDimension(size.substr(separator + 1));
            if (options.width < 1100 || options.height < 720 || options.width > 7680 || options.height > 4320)
                throw std::runtime_error("Size must be between 1100x720 and 7680x4320.");
        } else if (arg == "--screenshot") options.screenshot = value();
        else if (arg == "--capture-states") options.captures = value();
        else if (arg == "--assets") options.assets = value();
        else if (arg == "--hidden") options.hidden = true;
        else if (arg == "--demo") options.demo = true;
        else if (arg == "--autoplay") options.autoplay = true;
        else if (arg == "--no-animation") options.noAnimation = true;
        else if (arg == "--no-ambient-motion") options.noAmbientMotion = true;
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
                "  --size WxH        Initial window size (minimum 1100x720)\n"
                "  --screenshot FILE Save the final frame as PNG\n"
                "  --capture-states DIR Save menu, table, shop and end screens\n"
                "  --assets DIR      Override assets directory\n"
                "  --hidden          Create an invisible graphics window\n"
                "  --no-animation    Skip the start fade\n"
                "  --no-ambient-motion Disable decorative scene motion\n"
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
    explicit Window(bool hidden, int width, int height) {
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
        handle = glfwCreateWindow(width, height, "Jester Jack", nullptr, nullptr);
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
        style.WindowRounding = 14.0f;
        style.FrameRounding = 8.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.FramePadding = ImVec2(16, 8);
        style.WindowPadding = ImVec2(20, 16);
        style.ItemSpacing = ImVec2(12, 9);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.105f, 0.085f, 0.105f, 0.985f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.105f, 0.12f, 0.97f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.38f, 0.31f, 0.24f, 0.82f);
        style.Colors[ImGuiCol_Text] = cream;
        style.Colors[ImGuiCol_TextDisabled] = muted;
        style.Colors[ImGuiCol_Button] = ImVec4(0.24f, 0.17f, 0.25f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.34f, 0.25f, 0.34f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.19f, 0.13f, 0.20f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.19f, 0.15f, 0.18f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.22f, 0.25f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.32f, 0.24f, 0.30f, 1.0f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.25f, 0.20f, 0.23f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.31f, 0.25f, 0.28f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.36f, 0.29f, 0.32f, 1.0f);
        style.Colors[ImGuiCol_Separator] = ImVec4(0.42f, 0.34f, 0.25f, 0.65f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.09f, 0.07f, 0.09f, 0.8f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.40f, 0.32f, 0.28f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.51f, 0.42f, 0.34f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = gold;
        style.Colors[ImGuiCol_CheckMark] = gold;
        style.Colors[ImGuiCol_SliderGrab] = gold;
        style.Colors[ImGuiCol_SliderGrabActive] = cream;
        style.Colors[ImGuiCol_NavCursor] = gold;
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.16f, 0.11f, 0.16f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.25f, 0.17f, 0.24f, 1.0f);
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

struct ArtAssets {
    gfx::ArtTexture relics;
    gfx::ArtTexture hands;
    gfx::ArtTexture life;
    gfx::ArtTexture coins;
    explicit ArtAssets(const std::filesystem::path& assets)
        : relics(assets / "art/relics.tga"), hands(assets / "art/deck-hands.tga"),
          life(assets / "art/deck-life.tga"), coins(assets / "art/deck-coins.tga") {}
    const gfx::ArtTexture& deck(bj::StartDeck choice) const {
        return choice == bj::StartDeck::SecondLife ? life : choice == bj::StartDeck::Rich ? coins : hands;
    }
};

struct Session {
    bool menu = true;
    bool collection = false;
    bool deckSelection = false;
    int collectionFilter = 0;
    bj::Relic selectedRelic = bj::Relic::RoyalPurse;
    bj::StartDeck selectedDeck = bj::StartDeck::ExtraHand;
    float deckMotion = 0;
    int deckDirection = 1;
    bj::Relic inspectedRelic = bj::Relic::RoyalPurse;
    float relicHover[4]{};
    float collectionHover[24]{};
    int hoveredRelic = -1;
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

bool theatreButton(const char* label, ImVec2 size, ImVec4 tone = {.25f,.18f,.26f,1}) {
    const auto p = ImGui::GetCursorScreenPos();
    auto* draw = ImGui::GetWindowDrawList();
    if (size.x > 0 && size.y > 0)
        draw->AddRectFilled({p.x+1,p.y+4},{p.x+size.x+1,p.y+size.y+4},IM_COL32(0,0,0,75),8);
    ImGui::PushStyleColor(ImGuiCol_Button,tone);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{std::min(1.0f,tone.x+.07f),std::min(1.0f,tone.y+.07f),std::min(1.0f,tone.z+.07f),1});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,{tone.x*.78f,tone.y*.78f,tone.z*.78f,1});
    const bool pressed=ImGui::Button(label,size);
    ImGui::PopStyleColor(3);
    const auto end=ImGui::GetItemRectMax();
    draw->AddLine({p.x+9,p.y+1},{end.x-9,p.y+1},IM_COL32(255,237,201,40));
    return pressed;
}

float approach(float current, bool active) {
    const float amount=std::clamp(ImGui::GetIO().DeltaTime*12.0f,0.0f,1.0f);
    return current+((active?1.0f:0.0f)-current)*amount;
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
    const auto button = [&](const char* label, ImVec4 normal) {
        return theatreButton(label,{layout.width,layout.height},normal);
    };
    if (button("START", {.13f, .33f, .23f, 1}))
        session.deckSelection = true;
    ImGui::SetCursorPos({0, layout.step});
    if (button("OPTIONS", {.25f, .24f, .24f, 1}))
        session.settings = true;
    ImGui::SetCursorPos({0, 2 * layout.step});
    if (button("COLLECTION", {.28f, .17f, .35f, 1}))
        session.collection = true;
    ImGui::SetCursorPos({0, 3 * layout.step});
    if (button("EXIT", {.40f, .15f, .18f, 1}))
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
        case bj::Rarity::Common: return {0.78f, 0.76f, 0.67f, 1};
        case bj::Rarity::Rare: return {0.39f, 0.66f, 0.88f, 1};
        case bj::Rarity::Legendary: return {0.98f, 0.73f, 0.32f, 1};
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
    const auto tint=rarityColor(rarity);
    draw->AddRectFilled(origin, {origin.x + text.x + 20, origin.y + 32},
        ImGui::ColorConvertFloat4ToU32({tint.x*.18f,tint.y*.18f,tint.z*.18f,1}), 5);
    draw->AddRect(origin,{origin.x+text.x+20,origin.y+32},color,5,0,1);
    draw->AddText(ui.button, 26, {origin.x + 10, origin.y + 3}, color, label);
}

void drawCoinIcon(ImDrawList* draw, ImVec2 center, float radius, const Interface& ui) {
    const ImU32 rim = IM_COL32(166, 103, 27, 255);
    draw->AddCircleFilled({center.x - 7, center.y + 4}, radius, rim, 32);
    draw->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(gold), 32);
    draw->AddCircle(center, radius - 4, rim, 32, 2);
    const ImVec2 mark = ui.button->CalcTextSizeA(26, 1000, 0, "$");
    draw->AddText(ui.button, 26, {center.x - mark.x * .5f, center.y - mark.y * .5f}, rim, "$");
}

void drawChip(ImDrawList* draw, ImVec2 c, float r, const bj::RelicInfo& item,
              const ArtAssets& art, float tilt=0.0f) {
    const auto accent=ImGui::ColorConvertFloat4ToU32(rarityColor(item.rarity));
    draw->AddCircleFilled({c.x+2,c.y+6},r,IM_COL32(0,0,0,125),64);
    draw->AddCircleFilled(c,r,IM_COL32(45,34,35,255),64);
    draw->AddCircle(c,r-1,accent,64,1.5f);
    const float cs=std::cos(tilt),sn=std::sin(tilt);
    const auto corner=[&](float x,float y) {return ImVec2{c.x+x*cs-y*sn,c.y+x*sn+y*cs};};
    const int id=static_cast<int>(item.id),col=id%6,row=id/6;
    // The illustrations are packed slightly tighter vertically than the nominal grid.
    constexpr int rowTop[]={8,252,500,748};
    const ImVec2 a{(col*256+4.5f)/art.relics.width(),(rowTop[row]+.5f)/art.relics.height()};
    const ImVec2 b{(col*256+251.5f)/art.relics.width(),(rowTop[row]+247.5f)/art.relics.height()};
    const auto texture=static_cast<ImTextureID>(art.relics.id());
    const float image=r*.96f;
    draw->AddImageQuad(texture,corner(-image,-image),corner(image,-image),corner(image,image),corner(-image,image),
        a,{b.x,a.y},b,{a.x,b.y});
    draw->AddCircle(c,r*.88f,IM_COL32(255,237,203,30),64,1);
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
    const float boardY=display.y<800?236.0f:250.0f;
    return {sidebar, contentX, display.x - contentX - 20, boardY, display.y - boardY - 110, actionY};
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

void drawRelicStrip(const bj::Game& game, const TableLayout& layout, Session& session,
                    const Interface& ui, const ArtAssets& art) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    beginPanel("##relics", {layout.contentX, 12}, {layout.contentWidth, layout.boardY-18}, true);
    ImGui::PushFont(ui.small);
    ImGui::TextColored(gold, "YOUR COLLECTION   %d / 4", static_cast<int>(game.state().owned.size()));
    ImGui::PopFont();
    const bool compact=ImGui::GetIO().DisplaySize.y<800;
    const float radius=compact?48.0f:55.0f;
    const float width=layout.contentWidth/4;
    const float centerY=compact?77.0f:84.0f;
    session.hoveredRelic=-1;
    for (int slot = 0; slot < 4; ++slot) {
        ImGui::PushID(slot);
        ImGui::SetCursorPos({slot*width+4,25});
        const bool filled = static_cast<std::size_t>(slot) < game.state().owned.size();
        const auto* item = filled ? &relicInfo(game.state().owned[static_cast<std::size_t>(slot)]) : nullptr;
        ImGui::PushStyleColor(ImGuiCol_Header,{0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,{0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,{0,0,0,0});
        ImGui::Selectable("##chip",false,0,{width-8,compact?127.0f:141.0f});
        ImGui::PopStyleColor(3);
        const bool hovered=filled&&(ImGui::IsItemHovered()||ImGui::IsItemFocused());
        session.relicHover[slot]=approach(session.relicHover[slot],hovered);
        if(hovered) {session.inspectedRelic=item->id;session.hoveredRelic=slot;}
        const auto origin=ImGui::GetWindowPos();
        const ImVec2 center{origin.x+slot*width+width*.5f,origin.y+centerY-session.relicHover[slot]*5};
        auto* draw=ImGui::GetWindowDrawList();
        if (item) {
            drawChip(draw,center,radius,*item,art,-.055f*session.relicHover[slot]);
            const float textWidth=ui.button->CalcTextSizeA(22,1000,0,item->name).x;
            const float size=22*std::min(1.0f,(width-12)/textWidth);
            const float actual=ui.button->CalcTextSizeA(size,1000,0,item->name).x;
            draw->AddText(ui.button,size,{center.x-actual*.5f,origin.y+centerY+radius+6},
                ImGui::ColorConvertFloat4ToU32(cream),item->name);
        } else {
            draw->AddCircle(center,radius-4,IM_COL32(173,147,113,58),56,1.5f);
            draw->AddCircle(center,radius-10,IM_COL32(173,147,113,25),56,1);
            const std::string number=std::to_string(slot+1);
            const auto extent=ui.heading->CalcTextSizeA(36,1000,0,number.c_str());
            draw->AddText(ui.heading,36,{center.x-extent.x*.5f,center.y-extent.y*.5f},IM_COL32(187,162,129,62),number.c_str());
        }
        ImGui::PopID();
    }
    if(!game.state().owned.empty()&&!game.hasRelic(session.inspectedRelic)) session.inspectedRelic=game.state().owned.front();
    const auto origin=ImGui::GetWindowPos();
    const float plaqueY=compact?156.0f:170.0f;
    const ImVec2 p{origin.x,origin.y+plaqueY};
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(p,{p.x+layout.contentWidth,p.y+58},IM_COL32(24,20,25,235),8);
    draw->AddRect(p,{p.x+layout.contentWidth,p.y+58},IM_COL32(153,123,81,85),8);
    if(!game.state().owned.empty()) {
        const auto& item=relicInfo(session.inspectedRelic);
        draw->AddText(ui.button,22,{p.x+14,p.y+4},ImGui::ColorConvertFloat4ToU32(rarityColor(item.rarity)),item.name);
        const std::string tag=std::string(bj::rarityName(item.rarity))+"   /   "+std::to_string(item.cost)+" coins";
        const auto extent=ui.small->CalcTextSizeA(18,1000,0,tag.c_str());
        draw->AddText(ui.small,18,{p.x+layout.contentWidth-14-extent.x,p.y+7},ImGui::ColorConvertFloat4ToU32(muted),tag.c_str());
        const char* description=item.id==bj::Relic::SecondChance&&game.state().usedSecondChance
            ?"Rescue spent this round. Recharges next round.":item.description;
        const float textWidth=ui.small->CalcTextSizeA(18,2000,0,description).x;
        const float size=18*std::min(1.0f,(layout.contentWidth-28)/textWidth);
        draw->AddText(ui.small,size,{p.x+14,p.y+31},ImGui::ColorConvertFloat4ToU32(cream),description);
    } else {
        draw->AddText(ui.small,18,{p.x+16,p.y+18},ImGui::ColorConvertFloat4ToU32(muted),
            "Your cabinet is empty. Clear a round to visit the relic shop.");
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

    ImGui::PushStyleColor(ImGuiCol_ChildBg, {.17f, .12f, .22f, 1});
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
        ImGui::PushStyleColor(ImGuiCol_ChildBg, column ? ImVec4(.20f, .15f, .09f, 1) : ImVec4(.16f, .12f, .20f, 1));
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

void drawShop(bj::Game& game, Commands& commands, const Interface& ui, const ArtAssets& art) {
    const auto state = game.state();
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled({0, 0}, display, IM_COL32(8, 9, 18, 190));
    const float width = std::min(1100.0f, display.x - 64);
    const float height = std::min(790.0f, display.y - 48);
    beginPanel("##shop", {(display.x - width) * .5f, (display.y - height) * .5f}, {width, height});
    const ImVec2 header = ImGui::GetCursorScreenPos();
    ImGui::PushFont(ui.heading);
    ImGui::TextColored(cream, "The Curiosity Cabinet");
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
    ImGui::PushFont(ui.small);
    ImGui::TextColored(muted, "Round cleared. Two curiosities, one visit.   /   %d of 4 slots occupied",static_cast<int>(state.owned.size()));
    ImGui::PopFont();
    const float offerY = ImGui::GetCursorPosY() + 4;
    const float cardWidth = (width - 54) * .5f;
    const float offerHeight = height < 700 ? 238.0f : 268.0f;
    for (std::size_t i = 0; i < state.offers.size(); ++i) {
        const auto& item = relicInfo(state.offers[i]);
        const auto accent = rarityColor(item.rarity);
        ImGui::PushID(static_cast<int>(item.id));
        ImGui::SetCursorPos({20 + static_cast<float>(i) * (cardWidth + 14), offerY});
        ImGui::PushStyleColor(ImGuiCol_Border, {accent.x*.57f,accent.y*.57f,accent.z*.57f,1});
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {.13f, .105f, .12f, 1});
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{14,12});
        ImGui::BeginChild("offer", {cardWidth, offerHeight}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        const auto origin=ImGui::GetWindowPos();
        const float radius=height<700?65.0f:76.0f;
        drawChip(ImGui::GetWindowDrawList(),{origin.x+radius+18,origin.y+radius+22},radius,item,art);
        const float textX=radius*2+36;
        ImGui::SetCursorPos({textX,18});
        ImGui::PushFont(ui.button);
        ImGui::PushTextWrapPos(cardWidth-14);
        ImGui::TextColored(cream, "%s", item.name);
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        ImGui::SetCursorPosX(textX);
        rarityBadge(item.rarity, ui);
        ImGui::SetCursorPosX(textX);
        ImGui::PushFont(ui.small);
        ImGui::PushTextWrapPos(cardWidth-14);
        ImGui::TextWrapped("%s", item.description);
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        ImGui::SetCursorPos({14,offerHeight - 60});
        const bool owned = game.hasRelic(item.id);
        const bool purchased = game.boughtThisShop(item.id);
        const bool full = game.state().owned.size() >= 4;
        const bool affordable = game.state().coins >= item.cost;
        const std::string label = owned ? "EQUIPPED" : purchased ? "SOLD THIS VISIT" : full ? "4 / 4 EQUIPPED" :
            !affordable ? "NEED " + std::to_string(item.cost) + " COINS" :
            "BUY / " + std::to_string(item.cost) + " COINS";
        ImGui::PushFont(ui.button);
        ImGui::BeginDisabled(commands.blocked || commands.used || owned || purchased || full || !affordable);
        if (theatreButton(label.c_str(), {-1, 44},{.23f,.18f,.26f,1})) commands.apply([&]() { return game.buy(item.id); });
        ImGui::EndDisabled();
        ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }
    ImGui::SetCursorPos({20, offerY + offerHeight + 13});
    ImGui::Separator();
    ImGui::PushFont(ui.small);
    ImGui::TextColored(gold, "YOUR RELICS   /   Sell for half the price, rounded down");
    ImGui::PopFont();
    const float inventoryY=ImGui::GetCursorPosY()+4;
    const float inventoryWidth=(width-76)/4;
    for(std::size_t i=0;i<4;++i) {
        ImGui::PushID(100+static_cast<int>(i));
        const ImVec2 start{20+i*(inventoryWidth+12),inventoryY};
        ImGui::SetCursorPos(start);
        const auto origin=ImGui::GetCursorScreenPos();
        const bool filled=i<state.owned.size();
        auto* canvas=ImGui::GetWindowDrawList();
        canvas->AddRectFilled(origin,{origin.x+inventoryWidth,origin.y+144},IM_COL32(34,27,32,255),8);
        canvas->AddRect(origin,{origin.x+inventoryWidth,origin.y+144},IM_COL32(142,117,85,65),8);
        if(!filled) {
            canvas->AddCircle({origin.x+inventoryWidth*.5f,origin.y+54},30,IM_COL32(169,144,106,50),40,1);
            const char* empty="EMPTY";
            const float extent=ui.small->CalcTextSizeA(18,1000,0,empty).x;
            canvas->AddText(ui.small,18,{origin.x+(inventoryWidth-extent)*.5f,origin.y+98},ImGui::ColorConvertFloat4ToU32(muted),empty);
            ImGui::PopID();
            continue;
        }
        const auto& item=relicInfo(state.owned[i]);
        drawChip(canvas,{origin.x+46,origin.y+48},33,item,art);
        const float nameWidth=ui.button->CalcTextSizeA(22,1000,0,item.name).x;
        const float size=22*std::min(1.0f,(inventoryWidth-12)/nameWidth);
        canvas->AddText(ui.button,size,{origin.x+8,origin.y+85},ImGui::ColorConvertFloat4ToU32(cream),item.name);
        canvas->AddText(ui.small,18,{origin.x+88,origin.y+31},ImGui::ColorConvertFloat4ToU32(rarityColor(item.rarity)),bj::rarityName(item.rarity));
        ImGui::SetCursorPos({start.x+8,start.y+111});
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {12, 3});
        ImGui::BeginDisabled(commands.blocked || commands.used);
        const std::string label = "SELL / " + std::to_string(item.cost / 2);
        if (theatreButton(label.c_str(), {inventoryWidth-16, 27},{.29f,.18f,.17f,1})) commands.apply([&]() { return game.sell(item.id); });
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
        if(ImGui::IsItemHovered()) relicTooltip(item,ui);
        ImGui::PopID();
    }
    ImGui::SetCursorPos({20, height - 64});
    ImGui::PushFont(ui.button);
    ImGui::BeginDisabled(commands.blocked || commands.used);
    if (theatreButton("NEXT ROUND  /  SPACE", {350, 46},{.16f,.29f,.22f,1})) commands.apply([&]() { return game.leaveShop(); });
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
               const Interface& ui, const ArtAssets& art, GLFWwindow* window, const presentation::Presentation& timeline) {
    const auto state = settledState(timeline);
    const auto display = ImGui::GetIO().DisplaySize;
    const auto layout = tableLayout(display);
    if (state.phase == bj::Phase::RunLost && !timeline.busy()) {
        drawGameOver(game, commands, session, ui, window);
        return;
    }
    if (state.phase == bj::Phase::Shop) {
        drawShop(game, commands, ui, art);
        return;
    }
    drawSidebar(state, layout, session, commands, ui, display.y);
    drawRelicStrip(game, layout, session, ui, art);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    beginPanel("##actions", {controlsLeft(layout), layout.actionY + 42}, {460, 104}, true);
    ImGui::PushFont(ui.menuButton);
    ImGui::BeginDisabled(commands.blocked || commands.used);
    if (state.phase == bj::Phase::PlayerTurn) {
        if (theatreButton("HIT", {190, 78},{.11f,.31f,.24f,1})) commands.apply([&]() { return game.hit(); });
        ImGui::SameLine(206);
        if (theatreButton("STAND", {250, 78},{.33f,.22f,.13f,1})) commands.apply([&]() { return game.stand(); });
    } else if (state.phase == bj::Phase::Ready) {
        ImGui::SetCursorPosX(68);
        if (theatreButton("DEAL HAND", {320, 78},{.26f,.18f,.31f,1})) commands.apply([&]() { return game.deal(); });
    } else if (state.phase == bj::Phase::RoundOver) {
        ImGui::SetCursorPosX(68);
        if (theatreButton(state.roundScore >= state.goal ? "SHOP" : "CONTINUE", {320, 78},{.26f,.18f,.31f,1}))
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
void resetCamera(gfx::RenderSettings& settings) {
    const gfx::RenderSettings defaults;
    settings.yaw=defaults.yaw;
    settings.pitch=defaults.pitch;
    settings.distance=defaults.distance;
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
        ImGui::TextWrapped("Camera: right-drag the free table. Scroll to zoom. Double right-click to reset.");
        if (ImGui::Button("RESET VIEW")) resetCamera(settings);
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
    scene.deckDesign=state.startDeck==bj::StartDeck::SecondLife?1:state.startDeck==bj::StartDeck::Rich?2:0;
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

void drawCollection(Session& session, const Interface& ui, const ArtAssets& art) {
    if(!session.collection) return;
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},display,IM_COL32(12,7,24,200));
    const ImVec2 size{std::min(1240.0f,display.x-48),display.y-48};
    beginPanel("##collection",{(display.x-size.x)*.5f,24},size);
    ImGui::BeginDisabled(session.settings||session.rules||session.transitioning());
    heading("The Relic Collection",ui);
    ImGui::SetCursorPos({20,78});
    const char* filters[]={"ALL  /  24","COMMON  /  10","RARE  /  8","LEGENDARY  /  6"};
    for(int i=0;i<4;++i) {
        if(i) ImGui::SameLine();
        ImGui::PushID(i);
        if(theatreButton(filters[i],{160,36},session.collectionFilter==i?ImVec4(.33f,.25f,.18f,1):ImVec4(.20f,.15f,.21f,1)))
            session.collectionFilter=i;
        ImGui::PopID();
    }
    ImGui::SetCursorPos({20,128});
    const float detailWidth=display.x<1200?292.0f:320.0f;
    const float childHeight=size.y-208;
    ImGui::BeginChild("gallery",{size.x-detailWidth-52,childHeight},false);
    const float w=(ImGui::GetContentRegionAvail().x-36)/4;
    const float radius=std::min(57.0f,w*.34f);
    const float cellHeight=radius*2+70;
    int shown=0;
    for(const auto& item:bj::relicCatalog()) {
        if(session.collectionFilter && static_cast<int>(item.rarity)!=session.collectionFilter-1) continue;
        if(shown%4) ImGui::SameLine();
        ImGui::PushID(static_cast<int>(item.id));
        const auto pos=ImGui::GetCursorScreenPos();
        const bool selected=session.selectedRelic==item.id;
        const auto accent=rarityColor(item.rarity);
        ImGui::PushStyleColor(ImGuiCol_Header,{0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,{0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,{0,0,0,0});
        if(ImGui::Selectable("##relic",selected,0,{w,cellHeight})) session.selectedRelic=item.id;
        ImGui::PopStyleColor(3);
        const bool hovered=ImGui::IsItemHovered()||ImGui::IsItemFocused();
        auto& lift=session.collectionHover[static_cast<int>(item.id)];
        lift=approach(lift,hovered);
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos,{pos.x+w,pos.y+cellHeight},selected?IM_COL32(52,39,44,255):IM_COL32(31,25,31,170),8);
        draw->AddRect(pos,{pos.x+w,pos.y+cellHeight},selected?ImGui::ColorConvertFloat4ToU32(accent):IM_COL32(130,109,83,65),8,0,selected?1.5f:1.0f);
        drawChip(draw,{pos.x+w*.5f,pos.y+radius+12-lift*4},radius,item,art,-.05f*lift);
        const float available=w-14;
        const float nameWidth=ui.button->CalcTextSizeA(22,1000,0,item.name).x;
        const float fontSize=22*std::min(1.0f,available/nameWidth);
        centeredText(draw,ui.button,fontSize,pos.x+w*.5f,pos.y+radius*2+21,ImGui::ColorConvertFloat4ToU32(cream),item.name);
        centeredText(draw,ui.small,18,pos.x+w*.5f,pos.y+radius*2+46,ImGui::ColorConvertFloat4ToU32(accent),bj::rarityName(item.rarity));
        ImGui::PopID(); ++shown;
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{18,16});
    ImGui::BeginChild("detail",{detailWidth,childHeight},ImGuiChildFlags_Borders);
    const auto& item=relicInfo(session.selectedRelic);
    const auto pos=ImGui::GetCursorScreenPos();
    const float detailRadius=display.y<800?70.0f:86.0f;
    drawChip(ImGui::GetWindowDrawList(),{pos.x+(detailWidth-36)*.5f,pos.y+detailRadius+6},detailRadius,item,art);
    ImGui::Dummy({detailWidth-36,detailRadius*2+20});
    ImGui::PushFont(ui.heading); ImGui::TextWrapped("%s",item.name); ImGui::PopFont();
    rarityBadge(item.rarity,ui);
    ImGui::Spacing(); ImGui::PushFont(ui.small); ImGui::TextWrapped("%s",item.description); ImGui::PopFont();
    ImGui::Spacing(); ImGui::TextColored(gold,"BUY %d   /   SELL %d",item.cost,item.cost/2);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::SetCursorPosY(size.y-64);
    if(theatreButton("BACK",{180,44})) session.collection=false;
    ImGui::SameLine();
    ImGui::PushFont(ui.small);
    ImGui::TextColored(muted,"24 curiosities. Every one available from the first round.");
    ImGui::PopFont();
    ImGui::EndDisabled();
    ImGui::End();
}

float deckPanelHeight(ImVec2 display) {return std::min(680.0f,display.y-56);}

int deckIndex(bj::StartDeck deck) {
    return deck==bj::StartDeck::SecondLife?1:deck==bj::StartDeck::Rich?2:0;
}

bj::StartDeck deckAt(int index) {
    const bj::StartDeck decks[]={bj::StartDeck::ExtraHand,bj::StartDeck::SecondLife,bj::StartDeck::Rich};
    return decks[(index+3)%3];
}

void browseDeck(Session& session,int direction) {
    session.selectedDeck=deckAt(deckIndex(session.selectedDeck)+direction);
    session.deckDirection=direction;
    session.deckMotion=1;
}

struct DeckLayout {
    ImVec2 origin;
    float width;
    float height;
    ImVec2 card;
    ImVec2 cardSize;
    ImVec2 previous;
    ImVec2 next;
};

DeckLayout deckLayout(ImVec2 display) {
    const float width=std::min(1000.0f,display.x-56),height=deckPanelHeight(display);
    const bool compact=display.y<800;
    const ImVec2 size{compact?216.0f:244.0f,compact?324.0f:366.0f};
    const ImVec2 card{124,136};
    return {{(display.x-width)*.5f,(display.y-height)*.5f},width,height,card,size,
        {38,card.y+size.y*.5f-28},{card.x+size.x+28,card.y+size.y*.5f-28}};
}

void drawDeckSelection(bj::Game& game, Session& session, const Interface& ui, const ArtAssets& art) {
    if(!session.deckSelection) return;
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},display,IM_COL32(12,7,24,205));
    const auto layout=deckLayout(display);
    const float width=layout.width,height=layout.height;
    beginPanel("##decks",layout.origin,{width,height});
    ImGui::BeginDisabled(session.settings||session.rules||session.transitioning());
    heading("Choose Your Deck",ui);
    ImGui::PushFont(ui.small);
    ImGui::TextColored(muted,"A gift from the house. A different way to tempt fate.");
    ImGui::PopFont();
    const char* names[]={"LAST LAUGH","ENCORE","GOLDEN FOOL"};
    const char* tags[]={"ONE MORE CHANCE TO PLAY","THE SHOW GOES ON","A LITTLE HOUSE MONEY"};
    const char* descriptions[]={"An extra hand in every round.\nSix hands instead of five.","One revival for the whole run.\nReplay a failed round, keeping your relics and coins.","Begin with 10 extra coins.\nFive hands in every round."};
    const ImVec4 colors[]={blue,red,gold};
    ImGui::SetCursorPos(layout.previous);
    if(theatreButton("<",{56,56})) browseDeck(session,-1);
    ImGui::SetCursorPos(layout.next);
    if(theatreButton(">",{56,56})) browseDeck(session,1);
    if(!session.settings && !session.rules && !ImGui::GetIO().WantTextInput) {
        if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow,false)) browseDeck(session,-1);
        if(ImGui::IsKeyPressed(ImGuiKey_RightArrow,false)) browseDeck(session,1);
    }
    const int index=deckIndex(session.selectedDeck);
    auto* draw=ImGui::GetWindowDrawList();
    const float motion=session.deckMotion*session.deckMotion;
    const ImVec2 p{layout.origin.x+layout.card.x+session.deckDirection*motion*16,layout.origin.y+layout.card.y-motion*3};
    const float angle=session.deckDirection*motion*.022f;
    const ImVec2 center{p.x+layout.cardSize.x*.5f,p.y+layout.cardSize.y*.5f};
    const auto corner=[&](float x,float y) {
        return ImVec2{center.x+x*std::cos(angle)-y*std::sin(angle),center.y+x*std::sin(angle)+y*std::cos(angle)};
    };
    const float hw=layout.cardSize.x*.5f,hh=layout.cardSize.y*.5f;
    const ImVec2 corners[]={corner(-hw,-hh),corner(hw,-hh),corner(hw,hh),corner(-hw,hh)};
    ImVec2 shadow[4];
    for(int i=0;i<4;++i) shadow[i]={corners[i].x+6,corners[i].y+9};
    draw->AddConvexPolyFilled(shadow,4,IM_COL32(0,0,0,110));
    draw->AddImageQuad(static_cast<ImTextureID>(art.deck(session.selectedDeck).id()),corners[0],corners[1],corners[2],corners[3]);
    draw->AddPolyline(corners,4,ImGui::ColorConvertFloat4ToU32(gold),ImDrawFlags_Closed,1.5f);
    const std::string count=std::to_string(index+1)+"   /   3";
    centeredText(draw,ui.small,18,p.x+layout.cardSize.x*.5f,p.y+layout.cardSize.y+22,ImGui::ColorConvertFloat4ToU32(muted),count);
    const float textX=476;
    draw->AddText(ui.small,18,{layout.origin.x+textX,layout.origin.y+151},ImGui::ColorConvertFloat4ToU32(colors[index]),tags[index]);
    draw->AddText(ui.heading,50,{layout.origin.x+textX,layout.origin.y+193},ImGui::ColorConvertFloat4ToU32(cream),names[index]);
    draw->AddLine({layout.origin.x+textX,layout.origin.y+259},{layout.origin.x+width-52,layout.origin.y+259},IM_COL32(177,143,93,115));
    draw->AddText(ImGui::GetFont(),22,{layout.origin.x+textX,layout.origin.y+286},ImGui::ColorConvertFloat4ToU32(cream),descriptions[index],nullptr,width-textX-52);
    draw->AddText(ui.small,18,{layout.origin.x+textX,layout.origin.y+403},ImGui::ColorConvertFloat4ToU32(muted),
        "Your chosen back stays with you\nfor the entire run.");
    ImGui::SetCursorPos({36,height-90});
    if(theatreButton("BACK",{160,54})) session.deckSelection=false;
    ImGui::SetCursorPos({textX,height-90});
    ImGui::PushFont(ui.button);
    if(theatreButton("TAKE A SEAT",{width-textX-36,54},{.14f,.32f,.23f,1})) session.start(game);
    ImGui::PopFont();
    ImGui::EndDisabled();
    if(!session.settings&&!session.rules) session.deckMotion=std::max(0.0f,session.deckMotion-ImGui::GetIO().DeltaTime/.2f);
    ImGui::End();
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
        Window window(options.hidden, options.width, options.height);
        Interface interface(window.handle, assets);
        gfx::Renderer renderer(assets);
        ArtAssets art(assets);
        gfx::RenderSettings settings;
        settings.animate = !options.noAmbientMotion;
        const gfx::RenderSettings defaultView;
        bj::Game game(options.seed,options.demo ? options.deck : bj::StartDeck::Standard);
        Session session;
        HandFeedback feedback;
        presentation::Presentation timeline;
        timeline.reset(game.state());
        bool cameraDragging = false;
        bool smokeDealt=false, smokeStood=false, smokeCollection=false, smokeDeck=false, smokeRevived=false;
        int maxOwned=0,maxRound=1;
        int smokeDeckMasks[2]{};
        bool smokeSelectedCollection[2]{};
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
        int relicHoverResizeFrame = -1;
        double previousTime = glfwGetTime();
        std::set<std::string> captured;
        while (!glfwWindowShouldClose(window.handle)) {
            glfwPollEvents();
            if (options.smoke) {
                if (frame == 0 || frame == 34) glfwSetWindowSize(window.handle, 1100, 720);
                if (frame == 8 || frame == 300) glfwSetWindowSize(window.handle, 1440, 900);
                if (fullShopResizeFrame < 0 && game.state().phase == bj::Phase::Shop && game.state().owned.size() == 4)
                    fullShopResizeFrame = frame;
                if (frame == fullShopResizeFrame && fullShopResizeFrame >= 0)
                    glfwSetWindowSize(window.handle, 1100, 720);
                if (fullShopResizeFrame >= 0 && frame == fullShopResizeFrame + 20)
                    glfwSetWindowSize(window.handle, 1440, 900);
                if(relicHoverResizeFrame<0&&!session.menu&&!timeline.busy()&&!game.state().owned.empty()&&
                    game.state().phase!=bj::Phase::Shop&&game.state().phase!=bj::Phase::RunLost)
                    relicHoverResizeFrame=frame;
                if(frame==relicHoverResizeFrame&&relicHoverResizeFrame>=0) glfwSetWindowSize(window.handle,1100,720);
                if(relicHoverResizeFrame>=0&&frame==relicHoverResizeFrame+45) glfwSetWindowSize(window.handle,1440,900);
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
                const auto menu=menuLayout(io.DisplaySize);
                const auto click=[&](int down,ImVec2 point,int button=0) {
                    if(frame==down||frame==down+1) {
                        io.AddMousePosEvent(point.x,point.y);
                        io.AddMouseButtonEvent(button,frame==down);
                    }
                };
                if(!options.demo) {
                    if(frame==1||frame==6||frame==20) io.AddKeyEvent(ImGuiKey_F1,true);
                    if(frame==2||frame==7||frame==21) io.AddKeyEvent(ImGuiKey_F1,false);
                    click(10,{menu.origin.x+menu.width*.5f,menu.origin.y+2*menu.step+menu.height*.5f});
                    click(50,{menu.origin.x+menu.width*.5f,menu.origin.y+2*menu.step+menu.height*.5f});
                    const float collectionX=(io.DisplaySize.x-std::min(1240.0f,io.DisplaySize.x-48))*.5f;
                    click(12,{collectionX+20+3*172+80,120});
                    click(14,{collectionX+100,220});
                    click(52,{collectionX+20+3*172+80,120});
                    click(54,{collectionX+100,220});
                    if(frame==16||frame==24||frame==48||frame==56) io.AddKeyEvent(ImGuiKey_Escape,true);
                    if(frame==17||frame==25||frame==49||frame==57) io.AddKeyEvent(ImGuiKey_Escape,false);
                    click(18,{menu.origin.x+menu.width*.5f,menu.origin.y+menu.height*.5f});
                    click(58,{menu.origin.x+menu.width*.5f,menu.origin.y+menu.height*.5f});
                    const auto deck=deckLayout(io.DisplaySize);
                    const ImVec2 next{deck.origin.x+deck.next.x+28,deck.origin.y+deck.next.y+28};
                    // An outside-panel arrow click must stay blocked while settings are open.
                    click(22,{deck.origin.x+deck.previous.x+28,deck.origin.y+deck.previous.y+28});
                    for(const int step:{26,30,32,36,40,44}) click(step,next);
                    click(62,{deck.origin.x+476+(deck.width-512)*.5f,deck.origin.y+deck.height-63});
                }
                if(frame==124 || frame==126 || frame==128 || frame==130) {
                    const auto table=tableLayout(io.DisplaySize);
                    io.AddMousePosEvent(table.contentX+table.contentWidth*.65f+(frame>=126?40:0),table.boardY+120);
                    if(frame==124) io.AddMouseButtonEvent(1,true);
                    if(frame==128) io.AddMouseButtonEvent(1,false);
                    if(frame==130) io.AddMouseWheelEvent(0,1);
                }
                const auto table=tableLayout(io.DisplaySize);
                const ImVec2 freePoint{table.contentX+table.contentWidth*.65f+40,table.boardY+120};
                click(134,freePoint,1);click(138,freePoint,1);
                if(frame==142||frame==144||frame==146) {
                    io.AddMousePosEvent(freePoint.x+(frame>=144?40:0),freePoint.y);
                    if(frame==142) io.AddMouseButtonEvent(0,true);
                    if(frame==146) io.AddMouseButtonEvent(0,false);
                }
                if(frame>340&&!session.menu&&!game.state().owned.empty()&&game.state().phase!=bj::Phase::Shop&&game.state().phase!=bj::Phase::RunLost)
                    io.AddMousePosEvent(table.contentX+table.contentWidth*.125f,io.DisplaySize.y<800?89.0f:96.0f);
                if(frame>=160 && frame%20==0 && !session.menu && !timeline.busy() &&
                    game.state().phase!=bj::Phase::Shop && game.state().phase!=bj::Phase::RunLost)
                    io.AddKeyEvent(ImGuiKey_Space,true);
                if(frame>=161 && frame%20==1) io.AddKeyEvent(ImGuiKey_Space,false);
            }
            ImGui::NewFrame();
            const bool overlay=session.menu || session.settings || session.rules || session.collection || session.deckSelection || session.transitioning();
            const bool wasBusy=timeline.busy();
            Commands commands{overlay || wasBusy,false,&game,&timeline};
            if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)) {
                if(session.rules) session.rules=false;
                else if(session.settings) session.settings=false;
                else if(session.collection) session.collection=false;
                else if(session.deckSelection) session.deckSelection=false;
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
            else drawTable(game,commands,session,feedback,interface,art,window.handle,timeline);
            drawDeckSelection(game,session,interface,art);
            drawCollection(session,interface,art);
            drawSettings(session,settings,interface);
            drawRules(session,interface);
            ImGui::EndDisabled();
            const auto& io=ImGui::GetIO();
            const auto layout=tableLayout(io.DisplaySize);
            const bool freeTable=!session.menu && !session.settings && !session.rules && !session.transitioning() &&
                timeline.visible().phase!=bj::Phase::Shop && timeline.visible().phase!=bj::Phase::RunLost &&
                io.MousePos.x>layout.contentX && io.MousePos.y>layout.boardY && io.MousePos.y<layout.actionY &&
                !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
            if(!ImGui::IsMouseDown(1) || !freeTable) cameraDragging=false;
            if(freeTable && ImGui::IsMouseDoubleClicked(1)) {resetCamera(settings);cameraDragging=false;}
            else if(freeTable && ImGui::IsMouseClicked(1)) cameraDragging=true;
            if(cameraDragging && freeTable && ImGui::IsMouseDragging(1,3)) {
                settings.yaw=std::clamp(settings.yaw-io.MouseDelta.x*.20f,-35.0f,35.0f);
                settings.pitch=std::clamp(settings.pitch+io.MouseDelta.y*.16f,32.0f,68.0f);
            }
            if(freeTable && io.MouseWheel!=0) settings.distance=std::clamp(settings.distance-io.MouseWheel*.5f,8.0f,14.0f);
            if(options.smoke) {
                smokeCollection=smokeCollection||session.collection;
                smokeDeck=smokeDeck||session.deckSelection;
                const int sizeIndex=io.DisplaySize.y<800?0:1;
                if(session.deckSelection) smokeDeckMasks[sizeIndex]|=1<<deckIndex(session.selectedDeck);
                if(session.collection&&session.collectionFilter==3&&session.selectedRelic==bj::Relic::SecondChance)
                    smokeSelectedCollection[sizeIndex]=true;
                smokeDealt=smokeDealt||game.state().handsLeft<game.state().maxHands;
                smokeStood=smokeStood||game.state().outcome!=bj::Outcome::None;
                smokeRevived=smokeRevived||game.state().revivalsUsed>0;
                maxOwned=std::max(maxOwned,static_cast<int>(game.state().owned.size()));
                maxRound=std::max(maxRound,game.state().round);
                if(!options.demo&&((frame==2&&!session.settings)||(frame==8&&session.settings))) throw std::runtime_error("Settings input failed");
                if(!options.demo&&frame==24&&(session.selectedDeck!=options.deck||session.transitioning()))
                    throw std::runtime_error("Deck controls accepted input behind settings");
                if(!options.demo&&frame==24&&(!session.deckSelection||session.settings))
                    throw std::runtime_error("Escape closed the deck carousel instead of settings");
                if(frame==129&&std::abs(settings.yaw-defaultView.yaw)<1) throw std::runtime_error("Right drag did not rotate camera");
                if(frame==131&&settings.distance>=defaultView.distance) throw std::runtime_error("Mouse wheel did not zoom camera");
                if(frame==140&&(std::abs(settings.yaw-defaultView.yaw)>.01f||std::abs(settings.pitch-defaultView.pitch)>.01f||
                    std::abs(settings.distance-defaultView.distance)>.01f))
                    throw std::runtime_error("Double right-click did not reset camera");
                if(frame==148&&(std::abs(settings.yaw-defaultView.yaw)>.01f||std::abs(settings.pitch-defaultView.pitch)>.01f))
                    throw std::runtime_error("Left drag changed camera");
            }
            if(options.smoke) {
                if(frame==150) {settings.yaw=-35;settings.pitch=32;settings.distance=8;}
                if(frame==154) {settings.yaw=35;settings.pitch=68;settings.distance=14;}
                if(frame==158) resetCamera(settings);
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
                const std::string suffix=height<800?"-small":"-large";
                if(session.deckSelection) {
                    const char* names[]={"hands","life","coins"};
                    capture(std::string("deck-")+names[deckIndex(session.selectedDeck)]+suffix);
                    if(session.deckMotion>.8f) capture("carousel-motion"+suffix);
                }
                if(session.collection&&session.collectionFilter==3&&session.selectedRelic==bj::Relic::SecondChance)
                    capture("collection-selected"+suffix);
                if(!session.menu&&!session.settings&&!session.rules&&session.hoveredRelic>=0&&
                    session.relicHover[session.hoveredRelic]>.7f&&game.state().phase!=bj::Phase::Shop&&game.state().phase!=bj::Phase::RunLost)
                    capture("relic-hover"+suffix);
                if(name=="shop-full"||name=="table"||name=="collection"||name=="menu") capture(name+suffix);
                if(session.collection && session.collectionFilter) capture("collection-filter");
                const auto& visible=timeline.visible();
                const auto hand = bj::evaluateHand(visible.player);
                if(timeline.rewardStep()>0 && !visible.score.appliedEffects.empty())
                    capture("applied-relics-"+std::to_string(timeline.rewardStep()));
                if(visible.owned.size()==4 && visible.phase!=bj::Phase::Shop) {
                    capture("relics-full");
                    capture("relics-full"+suffix);
                }
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
            if (options.smoke && !options.captures.empty() && (frame == 65 || frame == 78 || frame == 91))
                saveScreenshot(options.captures / ("fade-" + std::to_string(frame) + ".png"), width, height);
            if(options.smoke && !options.captures.empty() && frame==275)
                saveScreenshot(options.captures/"table-small.png",width,height);
            if(options.smoke && !options.captures.empty() && (frame==152||frame==156))
                saveScreenshot(options.captures/(frame==152?"camera-low-close.png":"camera-high-far.png"),width,height);
            ++frame;
            const bool finished = options.frames > 0 && frame >= options.frames;
            if (finished && !options.screenshot.empty()) saveScreenshot(options.screenshot, width, height);
            glfwSwapBuffers(window.handle);
            if (finished) {
                if(options.smoke && !options.demo && frame>=400 && (!smokeCollection || !smokeDeck || !smokeDealt || !smokeStood))
                    throw std::runtime_error("Smoke traversal incomplete");
                if(options.smoke&&!options.demo&&frame>=400&&
                    (smokeDeckMasks[0]!=7||smokeDeckMasks[1]!=7||!smokeSelectedCollection[0]||!smokeSelectedCollection[1]))
                    throw std::runtime_error("Art screen traversal incomplete");
                if(options.smoke && !options.demo && options.deck==bj::StartDeck::SecondLife && frame>=2000 && !smokeRevived)
                    throw std::runtime_error("Revival traversal incomplete");
                break;
            }
        }
        std::cout << "Rendered frames: " << frame << '\n';
        if(options.smoke) std::cout << "Smoke: collection="<<smokeCollection<<" decks="<<smokeDeck
            <<" dealt="<<smokeDealt<<" resolved="<<smokeStood<<" revived="<<smokeRevived
            <<" max relics="<<maxOwned<<" max round="<<maxRound
            <<" deck masks="<<smokeDeckMasks[0]<<'/'<<smokeDeckMasks[1]
            <<" selected collection="<<smokeSelectedCollection[0]<<'/'<<smokeSelectedCollection[1]<<'\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Jester Jack: " << error.what() << '\n';
        return 1;
    }
}
