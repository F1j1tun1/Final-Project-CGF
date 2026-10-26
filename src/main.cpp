#include "presentation/ScoreHistory.hpp"
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
    bool cameraOverride = false;
    float light = 1.2f;
    bool lightSmoke = false;
    float cameraYaw=0, cameraPitch=32, cameraDistance=9.1f;
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
        } else if (arg == "--camera") {
            std::istringstream input(value());
            char comma1=0,comma2=0;
            if(!(input>>options.cameraYaw>>comma1>>options.cameraPitch>>comma2>>options.cameraDistance)
                || comma1!=',' || comma2!=',' || !input.eof()
                || !std::isfinite(options.cameraYaw) || !std::isfinite(options.cameraPitch) || !std::isfinite(options.cameraDistance)
                || std::abs(options.cameraYaw)>35 || options.cameraPitch<32 || options.cameraPitch>68
                || options.cameraDistance<gfx::minZoom || options.cameraDistance>gfx::maxZoom)
                throw std::runtime_error("Camera must be yaw,pitch,distance within -35..35,32..68,3.5..14.");
            options.cameraOverride=true;
        } else if (arg == "--light") {
            options.light=std::stof(value());
            if(!std::isfinite(options.light)||options.light<gfx::minLight||options.light>gfx::maxLight)
                throw std::runtime_error("Light must be within .55..2.5.");
        } else if(arg=="--light-smoke") options.lightSmoke=true;
        else if (arg == "--screenshot") options.screenshot = value();
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
                "  --camera Y,P,D    Diagnostic orbit: yaw,pitch,distance\n"
                "  --light VALUE     Lamp intensity (.55..2.5)\n"
                "  --light-smoke     Exercise live light slider at three values\n"
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
        // Keep capture dimensions exact on Windows, including full desktop sizes.
        if (hidden) glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
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
        style.WindowRounding = 8.0f;
        style.FrameRounding = 5.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.FramePadding = ImVec2(16, 8);
        style.WindowPadding = ImVec2(20, 16);
        style.ItemSpacing = ImVec2(12, 9);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.085f, 0.09f, 0.985f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.075f, 0.11f, 0.115f, 0.97f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.38f, 0.31f, 0.24f, 0.82f);
        style.Colors[ImGuiCol_Text] = cream;
        style.Colors[ImGuiCol_TextDisabled] = muted;
        style.Colors[ImGuiCol_Button] = ImVec4(0.12f, 0.19f, 0.20f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.30f, 0.30f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.09f, 0.15f, 0.16f, 1.0f);
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
    ImVec2 lightMin{},lightMax{};
    presentation::ScoreHistory history;
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
    float gameOverElapsed = 0.0f;
    bool gameOverFocused = false;
    int relicReactionStep = -1;
    float relicReactionElapsed = 0.0f;
    float bustReaction = 0.0f;
    bool showingBust = false;

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
            history.reset();
            menu = false;
            started = true;
            switched = true;
        }
        if (fade >= 1.0f) fade = -1.0f;
    }
    float darkness() const {
        return transitioning() ? std::clamp(1.0f - std::abs(2.0f * fade - 1.0f), 0.0f, 1.0f) : 0.0f;
    }
    void updateGameOver(float dt, bool showing) {
        if (showing) gameOverElapsed = std::min(3.0f, gameOverElapsed + dt);
        else {
            gameOverElapsed = 0.0f;
            gameOverFocused = false;
        }
    }
    void updateRelicReaction(float dt, const presentation::Presentation& timeline) {
        const auto& state=timeline.visible();
        const int step=timeline.rewardStep();
        const bool active=timeline.busy() && step>0 &&
            step<=static_cast<int>(state.score.appliedEffects.size());
        if(!active) {
            relicReactionStep=-1;
            relicReactionElapsed=0;
        } else if(relicReactionStep!=step) {
            relicReactionStep=step;
            relicReactionElapsed=0;
        } else relicReactionElapsed+=dt;
    }
    void updateBustReaction(float dt, const bj::Snapshot& state) {
        const bool bust=(state.phase==bj::Phase::RoundOver || state.phase==bj::Phase::RunLost) &&
            state.outcome==bj::Outcome::Loss && bj::evaluateHand(state.player).bust;
        if(bust && !showingBust) bustReaction=1.0f;
        else if(!bust) bustReaction=0.0f;
        else bustReaction=std::max(0.0f,bustReaction-dt/.8f);
        showingBust=bust;
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
        case bj::Rarity::Common: return {0.70f, 0.72f, 0.72f, 1};
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

struct RelicRowStyle {
    float radius;
    float nameSize;
    float bodySize;
    float raritySize;
    float height;
};

RelicRowStyle relicRowStyle(float contentWidth) {
    const float column=contentWidth/4;
    if(column<235) return {38,22,18,16,208};
    if(column<320) return {44,26,20,18,208};
    return {44,28,22,18,192};
}

TableLayout tableLayout(ImVec2 display) {
    const float sidebar = std::clamp(display.x * .24f, 280.0f, 340.0f);
    const float contentX = sidebar + 24;
    const float actionY = display.y - 154;
    const float contentWidth=display.x-contentX-20;
    const float boardY=relicRowStyle(contentWidth).height;
    return {sidebar, contentX, contentWidth, boardY, display.y - boardY - 110, actionY};
}

void drawGameplayShading(const Session& session, const presentation::Presentation& timeline) {
    const auto phase=timeline.visible().phase;
    if(session.menu || phase==bj::Phase::Shop || (phase==bj::Phase::RunLost && !timeline.busy())) return;
    const auto display=ImGui::GetIO().DisplaySize;
    const auto layout=tableLayout(display);
    auto* draw=ImGui::GetBackgroundDrawList();
    constexpr ImU32 shade=IM_COL32(0,0,0,48),clear=IM_COL32(0,0,0,0);
    draw->AddRectFilledMultiColor({layout.contentX,0},{display.x,140},shade,shade,clear,clear);
    draw->AddRectFilledMultiColor({layout.contentX,display.y-140},display,clear,clear,shade,shade);
}

const char* ownedRelicDescription(const bj::RelicInfo& item, bool rescueSpent) {
    return item.id==bj::Relic::SecondChance && rescueSpent
        ?"Rescue spent this round. Win the rescued hand: +1.0 multiplier.":item.description;
}

struct WrappedRelicText {
    std::string text;
    float longestWord=0;
};

WrappedRelicText wrapRelicWords(ImFont* font, float size, float width, const char* text) {
    WrappedRelicText wrapped;
    std::istringstream words(text);
    std::string word,line;
    while(words>>word) {
        wrapped.longestWord=std::max(wrapped.longestWord,font->CalcTextSizeA(size,10000,0,word.c_str()).x);
        const std::string candidate=line.empty()?word:line+" "+word;
        if(!line.empty() && font->CalcTextSizeA(size,10000,0,candidate.c_str()).x>width) {
            wrapped.text+=line+'\n';
            line=word;
        } else line=candidate;
    }
    wrapped.text+=line;
    return wrapped;
}

void verifyRelicTextLayout(const Interface& ui, const TableLayout& layout, ImVec2 display) {
    const auto style=relicRowStyle(layout.contentWidth);
    const float textWidth=layout.contentWidth/4-style.radius*2-22;
    const float nameSize=style.nameSize,bodySize=style.bodySize;
    auto* bodyFont=ImGui::GetIO().FontDefault;
    float maxBottom=0,longestWord=0;
    const char* longest="";
    for(const auto& item:bj::relicCatalog()) {
        const float nameHeight=ui.button->CalcTextSizeA(nameSize,1000,textWidth,item.name).y;
        if(nameHeight>nameSize*2+.5f) throw std::runtime_error(std::string("Relic name exceeds two lines: ")+item.name);
        for(const bool spent:{false,true}) {
            const auto description=wrapRelicWords(bodyFont,bodySize,textWidth,ownedRelicDescription(item,spent));
            if(description.longestWord>textWidth+.5f)
                throw std::runtime_error(std::string("Relic description word exceeds column: ")+item.name);
            longestWord=std::max(longestWord,description.longestWord);
            const float descriptionHeight=bodyFont->CalcTextSizeA(bodySize,10000,0,description.text.c_str()).y;
            const float bottom=14+nameHeight+4+style.raritySize+8+descriptionHeight;
            if(bottom>layout.boardY-6) throw std::runtime_error(std::string("Relic description exceeds row: ")+item.name);
            if(bottom>maxBottom) {maxBottom=bottom;longest=item.name;}
        }
    }
    std::cout<<"Relic layout "<<static_cast<int>(display.x)<<'x'<<static_cast<int>(display.y)
        <<": max text bottom "<<maxBottom<<" / "<<layout.boardY<<" ("<<longest
        <<"), longest word "<<longestWord<<" / "<<textWidth<<'\n';
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

void drawRelicStrip(const bj::Snapshot& state, const presentation::Presentation& timeline,
                    const TableLayout& layout, Session& session, const Interface& ui, const ArtAssets& art) {
    session.hoveredRelic=-1;
    session.updateRelicReaction(0,timeline);
    const auto* effect=session.relicReactionStep>0
        ?&state.score.appliedEffects[static_cast<std::size_t>(session.relicReactionStep-1)]:nullptr;
    const float duration=.5f+(.07f-.5f)*std::clamp(session.animationSpeed,0.0f,1.0f);
    const float progress=std::clamp(session.relicReactionElapsed/duration,0.0f,1.0f);
    const float pulse=effect?std::max(0.0f,std::sin(progress*3.14159265f)):0.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.0f);
    beginPanel("##relics", {layout.contentX, 0}, {layout.contentWidth, layout.boardY}, true,
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    const float width=layout.contentWidth/4;
    const auto style=relicRowStyle(layout.contentWidth);
    const float radius=style.radius;
    const float centerY=radius+30;
    for (int slot = 0; slot < 4; ++slot) {
        const auto origin=ImGui::GetWindowPos();
        auto* draw=ImGui::GetWindowDrawList();
        const float centerX=origin.x+slot*width+radius+5;
        const bool filled = static_cast<std::size_t>(slot) < state.owned.size();
        if(!filled) {
            session.relicHover[slot]=approach(session.relicHover[slot],false);
            const ImVec2 center{centerX,origin.y+centerY};
            draw->AddCircle(center,radius-3,IM_COL32(190,178,154,75),64,1.25f);
            draw->AddCircle(center,radius-9,IM_COL32(190,178,154,26),64,1);
            const std::string number=std::to_string(slot+1);
            const auto extent=ui.heading->CalcTextSizeA(30,1000,0,number.c_str());
            draw->AddText(ui.heading,30,{center.x-extent.x*.5f,center.y-extent.y*.5f},
                IM_COL32(190,178,154,70),number.c_str());
            continue;
        }
        const auto& item=relicInfo(state.owned[static_cast<std::size_t>(slot)]);
        ImGui::PushID(slot);
        ImGui::SetCursorPos({slot*width+1,centerY-radius-4});
        ImGui::PushStyleColor(ImGuiCol_Header,{0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,{0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,{0,0,0,0});
        ImGui::Selectable("##chip",false,0,{radius*2+8,radius*2+8});
        ImGui::PopStyleColor(3);
        const bool hovered=!session.settings&&!session.rules&&!session.transitioning()&&
            (ImGui::IsItemHovered()||(ImGui::GetIO().NavVisible&&ImGui::IsItemFocused()));
        session.relicHover[slot]=approach(session.relicHover[slot],hovered);
        if(hovered) {session.inspectedRelic=item.id;session.hoveredRelic=slot;}
        const bool reacting=effect && effect->relic==item.id;
        const float lift=session.relicHover[slot]*4+(reacting?pulse*9:0);
        const ImVec2 center{centerX,origin.y+centerY-lift};
        drawChip(draw,center,radius,item,art,-.055f*session.relicHover[slot]+(reacting?pulse*.025f:0));
        const float textX=origin.x+slot*width+radius*2+14;
        const float textWidth=width-radius*2-22;
        const float nameSize=style.nameSize,bodySize=style.bodySize;
        const float nameY=origin.y+14;
        const float nameHeight=ui.button->CalcTextSizeA(nameSize,1000,textWidth,item.name).y;
        draw->AddText(ui.button,nameSize,{textX,nameY},ImGui::ColorConvertFloat4ToU32(cream),
            item.name,nullptr,textWidth);
        const float rarityY=nameY+nameHeight+4;
        draw->AddText(ui.small,style.raritySize,{textX,rarityY},ImGui::ColorConvertFloat4ToU32(rarityColor(item.rarity)),
            bj::rarityName(item.rarity));
        auto* bodyFont=ImGui::GetIO().FontDefault;
        const auto description=wrapRelicWords(bodyFont,bodySize,textWidth,ownedRelicDescription(item,state.usedSecondChance));
        draw->AddText(bodyFont,bodySize,{textX,rarityY+style.raritySize+8},ImGui::ColorConvertFloat4ToU32(cream),
            description.text.c_str());
        if(hovered && (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) || ImGui::GetIO().NavVisible)) {
            ImGui::BeginTooltip();
            ImGui::PushFont(ui.small);
            ImGui::Text("Buy: %d coins    Sell: %d coins",item.cost,item.cost/2);
            ImGui::PopFont();
            ImGui::EndTooltip();
        }
        if(reacting) {
            const auto accent=rarityColor(item.rarity);
            draw->AddCircle(center,radius+3,ImGui::ColorConvertFloat4ToU32(
                {accent.x,accent.y,accent.z,.80f*pulse}),64,1+2*pulse);
            draw->AddCircle(center,radius+6,ImGui::ColorConvertFloat4ToU32(
                {accent.x,accent.y,accent.z,.20f*pulse}),64,1);
            std::vector<std::string> bonuses;
            if(effect->points) bonuses.push_back("+"+std::to_string(effect->points)+" POINTS");
            if(effect->multiplierBonus>0) {
                std::ostringstream value;
                value<<'+'<<std::setprecision(2)<<effect->multiplierBonus<<" MULT";
                bonuses.push_back(value.str());
            }
            if(effect->coins) bonuses.push_back("+"+std::to_string(effect->coins)+(effect->coins==1?" COIN":" COINS"));
            float widest=1;
            for(const auto& bonus:bonuses) widest=std::max(widest,ui.button->CalcTextSizeA(20,1000,0,bonus.c_str()).x);
            const float size=std::min(20.0f,20*(radius*2+4)/widest);
            const float popupY=origin.y+centerY+radius+8-pulse*3;
            const auto ink=ImGui::ColorConvertFloat4ToU32({accent.x,accent.y,accent.z,.45f+.55f*pulse});
            for(std::size_t line=0;line<bonuses.size();++line) {
                const auto& bonus=bonuses[line];
                const float extent=ui.button->CalcTextSizeA(size,1000,0,bonus.c_str()).x;
                const float x=center.x-extent*.5f;
                draw->AddText(ui.button,size,{x,popupY+static_cast<float>(line)*(size+2)},ink,bonus.c_str());
            }
        }
        ImGui::PopID();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
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

    ImGui::Spacing();
    ImGui::PushFont(ui.small);
    ImGui::TextColored(muted, "ROUND PROGRESS");
    ImGui::PopFont();
    ImGui::PushFont(ui.number);
    ImGui::TextColored(cream, "%d", state.roundScore);
    ImGui::PopFont();
    ImGui::SameLine(0, 10);
    ImGui::PushFont(ui.button);
    ImGui::TextColored(gold, "/ %d", state.goal);
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, gold);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, {.12f,.19f,.20f,1});
    ImGui::ProgressBar(std::min(1.0f, static_cast<float>(state.roundScore) / state.goal), {-1, 6}, "");
    ImGui::PopStyleColor(2);
    ImGui::PushFont(ui.small);
    if(state.roundScore >= state.goal) ImGui::TextColored(green, "TARGET REACHED");
    else ImGui::TextColored(muted, "%d points to the next round", state.goal-state.roundScore);
    ImGui::PopFont();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float half = (ImGui::GetContentRegionAvail().x - 8) * .5f;
    for (int column = 0; column < 2; ++column) {
        if (column) ImGui::SameLine();
        ImGui::PushID(column);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.075f, .115f, .12f, 1));
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
    ImGui::PushFont(ui.small);
    if (state.startDeck == bj::StartDeck::SecondLife) ImGui::TextColored(gold, "REVIVE: %d / 1", state.revivesLeft);
    ImGui::TextColored(muted, "RUN TOTAL  %d", state.totalScore);
    ImGui::Spacing();
    ImGui::TextColored(gold, "SCORE JOURNAL");
    const float journalHeight=std::max(60.0f,height-32-120-ImGui::GetCursorPosY());
    ImGui::BeginChild("score-journal",{-1,journalHeight},ImGuiChildFlags_None);
    if(session.history.records().empty()) {
        ImGui::TextColored(cream,"Your story starts here.");
        ImGui::TextWrapped("Finish a hand to record its score and the relics that contributed.");
    }
    int recordIndex=0;
    for(const auto& record:session.history.records()) {
        ImGui::PushID(recordIndex++);
        const auto& score=record.score;
        const char* result=record.outcome==bj::Outcome::Win?"WIN":
            record.outcome==bj::Outcome::Loss?"LOSS":"PUSH";
        ImGui::TextColored(muted,"R%d / HAND %d%s",record.round,record.hand,record.attempt>1?" / RETRY":"");
        ImGui::TextColored(record.outcome==bj::Outcome::Win?green:cream,
                           "%s   +%d pts / +%d coins",result,score.total,score.coins);
        if(record.outcome==bj::Outcome::Win) {
            ImGui::TextColored(cream,"(%d base + %d bonus) x %.2f",score.base,score.bonus,score.multiplier);
            for(const auto& effect:score.appliedEffects) {
                const auto& catalog=bj::relicCatalog();
                const auto item=std::find_if(catalog.begin(),catalog.end(),[&](const auto& info){return info.id==effect.relic;});
                if(item==catalog.end()) continue;
                std::ostringstream contribution;
                contribution<<item->name<<":";
                if(effect.points) contribution<<" +"<<effect.points<<" pts";
                if(effect.multiplierBonus>0) contribution<<" +"<<std::fixed<<std::setprecision(2)<<effect.multiplierBonus<<" mult";
                if(effect.coins) contribution<<" +"<<effect.coins<<" coins";
                ImGui::TextWrapped("%s",contribution.str().c_str());
            }
        } else ImGui::TextColored(muted,"No points awarded");
        if(record.roundScore>=record.goal) ImGui::TextColored(gold,"ROUND TARGET REACHED");
        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();
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

// Effects stay in the border gutter; rarity text remains the non-color signal.
void drawRarityFrame(ImDrawList* draw, ImVec2 origin, ImVec2 size, bj::Rarity rarity, bool animate) {
    const bool legendary=rarity==bj::Rarity::Legendary;
    const bool rare=rarity==bj::Rarity::Rare;
    const auto accent=rarityColor(rarity);
    const ImU32 ink=ImGui::ColorConvertFloat4ToU32(accent);
    const ImVec2 end{origin.x+size.x-2,origin.y+size.y-2};
    draw->AddRect({origin.x+2,origin.y+2},end,ink,5,0,legendary?2.5f:rare?1.5f:1.0f);
    if(!legendary&&!rare) return;
    if(legendary) draw->AddRect({origin.x+6,origin.y+6},{end.x-4,end.y-4},
                              IM_COL32(207,150,62,100),3,0,1);
    for(float side : {0.0f,1.0f}) {
        const float x=origin.x+8+side*(size.x-16),dir=side? -1.0f:1.0f;
        for(float bottom : {0.0f,1.0f}) {
            const float y=origin.y+8+bottom*(size.y-16),dy=bottom?-1.0f:1.0f;
            draw->AddLine({x,y},{x+dir*(legendary?26:15),y},ink,2);
            draw->AddLine({x,y},{x,y+dy*(legendary?26:15)},ink,2);
        }
    }
    if(rare) {
        const float t=animate?static_cast<float>(ImGui::GetTime()):0;
        const float x=origin.x+20+(size.x-40)*(.5f+.5f*std::sin(t*.32f));
        draw->AddRect({origin.x+4,origin.y+4},{end.x-2,end.y-2},IM_COL32(100,168,224,35),4,0,3);
        draw->AddLine({x-8,origin.y+3},{x+8,origin.y+3},IM_COL32(180,221,250,125),1.5f);
    }
    if(legendary) {
        const float t=animate?static_cast<float>(ImGui::GetTime()):0;
        const float x=origin.x+12+(size.x-24)*(.5f+.5f*std::sin(t*.45f));
        draw->AddLine({x-12,origin.y+3},{x+12,origin.y+3},IM_COL32(255,235,166,210),2);
        for(int i=0;i<3;++i) {
            const float px=origin.x+20+(size.x-40)*(i+1)/4;
            const float alpha=.45f+.15f*std::sin(t*.8f+i*2);
            const auto glow=ImGui::ColorConvertFloat4ToU32({1,.83f,.43f,alpha});
            draw->AddLine({px-3,end.y-4},{px+3,end.y-4},glow,1);
            draw->AddLine({px,end.y-7},{px,end.y-1},glow,1);
        }
    }
}

void drawShop(bj::Game& game, Commands& commands, const Interface& ui, const ArtAssets& art, bool animate) {
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
        ImGui::PushStyleColor(ImGuiCol_ChildBg, item.rarity==bj::Rarity::Legendary?ImVec4(.16f,.125f,.07f,1):ImVec4(.085f,.105f,.12f,1));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{14,12});
        ImGui::BeginChild("offer", {cardWidth, offerHeight}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        const auto origin=ImGui::GetWindowPos();
        drawRarityFrame(ImGui::GetWindowDrawList(),origin,{cardWidth,offerHeight},item.rarity,animate);
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
    const bool compact=display.y<800;
    const float fade=std::clamp(session.gameOverElapsed/.8f,0.0f,1.0f);
    const float darkness=fade*fade*(3.0f-2.0f*fade);
    const float appear=std::clamp((session.gameOverElapsed-.12f)/.68f,0.0f,1.0f);
    const float alpha=appear*appear*(3.0f-2.0f*appear);
    ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},display,
        IM_COL32(0,0,0,static_cast<int>(255*darkness)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    if(!session.gameOverFocused) {
        ImGui::SetNextWindowFocus();
        session.gameOverFocused=true;
    }
    beginPanel("##end",{0,0},display,true,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    auto* draw=ImGui::GetWindowDrawList();
    const float center=display.x*.5f;
    const auto white=IM_COL32(255,255,255,static_cast<int>(255*alpha));
    const auto pale=IM_COL32(210,210,210,static_cast<int>(255*alpha));
    const auto scarlet=IM_COL32(220,28,45,static_cast<int>(255*alpha));
    const auto text=[&](ImFont* font,float size,float x,float y,ImU32 color,const std::string& value) {
        const float width=font->CalcTextSizeA(size,10000,0,value.c_str()).x;
        draw->AddText(font,size,{x-width*.5f,y},color,value.c_str());
    };
    const float titleY=compact?74.0f:100.0f;
    const float titleSize=compact?148.0f:184.0f;
    text(ui.title,titleSize,center,titleY,white,"GAME OVER");
    draw->AddLine({center-34,titleY+titleSize+12},{center+34,titleY+titleSize+12},scarlet,2);
    const float statsY=titleY+titleSize+(compact?38.0f:50.0f);
    text(ui.small,18,center-160,statsY,pale,"ROUND REACHED");
    text(ui.small,18,center+160,statsY,pale,"TOTAL RUN SCORE");
    text(ui.number,compact?54.0f:64.0f,center-160,statsY+23,white,std::to_string(state.round));
    text(ui.number,compact?54.0f:64.0f,center+160,statsY+23,white,std::to_string(state.totalScore));
    if(state.revivesLeft>0)
        text(ui.small,16,center,statsY+94,pale,"ENCORE: ONE LIFE REMAINS");
    const float buttonWidth=340,buttonHeight=compact?54.0f:60.0f;
    const float buttonY=statsY+126,step=compact?68.0f:76.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,2);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1);
    ImGui::PushStyleColor(ImGuiCol_Text,{1,1,1,1});
    ImGui::PushStyleColor(ImGuiCol_Border,{1,1,1,1});
    ImGui::PushStyleColor(ImGuiCol_NavCursor,{1,1,1,1});
    ImGui::PushStyleColor(ImGuiCol_Button,{0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.54f,.025f,.055f,1});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,{.72f,.025f,.055f,1});
    ImGui::PushFont(ui.button);
    ImGui::BeginDisabled(commands.blocked || commands.used || session.settings || session.rules || session.transitioning());
    const auto button=[&](const char* label,float y) {
        ImGui::SetCursorPos({center-buttonWidth*.5f,y});
        const auto p=ImGui::GetCursorScreenPos();
        const bool mouse=ImGui::IsMouseHoveringRect(p,{p.x+buttonWidth,p.y+buttonHeight});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,mouse?ImVec4(.54f,.025f,.055f,1):ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,mouse?ImVec4(.72f,.025f,.055f,1):ImVec4(0,0,0,0));
        const bool clicked=ImGui::Button(label,{buttonWidth,buttonHeight});
        ImGui::PopStyleColor(2);
        return clicked;
    };
    const bool retry=button(state.revivesLeft>0?"REVIVE / SPACE":"RETRY / SPACE",buttonY);
    ImGui::SetItemDefaultFocus();
    if(retry) {
        if(state.revivesLeft>0) commands.apply([&]() {return game.revive();});
        else session.start(game);
    }
    if(button("MAIN MENU",buttonY+step)) session.menu=true;
    if(button("QUIT GAME",buttonY+step*2)) glfwSetWindowShouldClose(window,GLFW_TRUE);
    ImGui::EndDisabled();
    ImGui::PopFont();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);
    ImGui::End();
    ImGui::PopStyleVar(2);
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
               const Interface& ui, const ArtAssets& art, GLFWwindow* window, const presentation::Presentation& timeline, bool animate) {
    const auto state = settledState(timeline);
    const auto display = ImGui::GetIO().DisplaySize;
    const auto layout = tableLayout(display);
    if (state.phase == bj::Phase::RunLost && !timeline.busy()) {
        drawGameOver(game, commands, session, ui, window);
        return;
    }
    if (state.phase == bj::Phase::Shop) {
        drawShop(game, commands, ui, art, animate);
        return;
    }
    drawSidebar(state, layout, session, commands, ui, display.y);
    drawRelicStrip(state, timeline, layout, session, ui, art);
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
        ImGui::SliderFloat("Light", &settings.lightIntensity, gfx::minLight, gfx::maxLight);
        session.lightMin=ImGui::GetItemRectMin();
        session.lightMax={session.lightMin.x+ImGui::CalcItemWidth(),ImGui::GetItemRectMax().y};
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

gfx::Scene makeScene(const presentation::Presentation& timeline, float time, bool menu, float bustReaction) {
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
    scene.bustReaction=bustReaction;
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
        settings.lightIntensity=options.light;
        if(options.cameraOverride) {settings.yaw=options.cameraYaw;settings.pitch=options.cameraPitch;settings.distance=options.cameraDistance;}
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
        std::set<std::pair<int,int>> checkedRelicLayouts;
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
            session.history.observe(timeline.visible(),timeline.busy());
            session.updateGameOver(dt,!session.menu && !timeline.busy() && timeline.visible().phase==bj::Phase::RunLost);
            session.updateRelicReaction(!session.menu&&!session.settings&&!session.rules?dt:0,timeline);
            session.updateBustReaction(dt,timeline.visible());
            if(options.lightSmoke) {
                if(frame==8) session.settings=true;
                if(frame==45) session.settings=false;
            }
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            if(options.lightSmoke) {
                auto& input=ImGui::GetIO();
                for(int phase=0;phase<3;++phase) {
                    const int down=12+phase*10;
                    if(frame==down||frame==down+1) {
                        const float fraction=phase*.5f;
                        input.AddMousePosEvent(session.lightMin.x+3+(session.lightMax.x-session.lightMin.x-6)*fraction,
                            (session.lightMin.y+session.lightMax.y)*.5f);
                        input.AddMouseButtonEvent(0,frame==down);
                    }
                    if(frame==down+4) {
                        const float expected=gfx::minLight+(gfx::maxLight-gfx::minLight)*phase*.5f;
                        if(std::abs(settings.lightIntensity-expected)>.035f)
                            throw std::runtime_error("Light slider input failed");
                        std::cout<<"Live light slider: "<<settings.lightIntensity<<'\n';
                    }
                }
            }
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
                if(frame>340&&!session.menu&&!game.state().owned.empty()&&game.state().phase!=bj::Phase::Shop&&game.state().phase!=bj::Phase::RunLost) {
                    const auto relicStyle=relicRowStyle(table.contentWidth);
                    io.AddMousePosEvent(table.contentX+relicStyle.radius+5,relicStyle.radius+30);
                }
                if(frame>=160 && frame%20==0 && !session.menu && !timeline.busy() &&
                    game.state().phase!=bj::Phase::Shop && game.state().phase!=bj::Phase::RunLost)
                    io.AddKeyEvent(ImGuiKey_Space,true);
                if(frame>=161 && frame%20==1) io.AddKeyEvent(ImGuiKey_Space,false);
            }
            ImGui::NewFrame();
            if(!options.captures.empty()) {
                const auto display=ImGui::GetIO().DisplaySize;
                const auto size=std::make_pair(static_cast<int>(display.x),static_cast<int>(display.y));
                if(checkedRelicLayouts.insert(size).second) verifyRelicTextLayout(interface,tableLayout(display),display);
            }
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
                    if(session.gameOverElapsed>=1.1f) {
                        if(game.state().revivesLeft) commands.apply([&](){return game.revive();});
                        else session.start(game);
                    }
                } else if(session.menu && !options.smoke) session.start(game);
                else autoplayStep(game,commands);
            }
            feedback.observe(settledState(timeline),dt,!session.menu && !session.transitioning());
            // Space is handled above; prevent focused buttons from receiving it again.
            ImGui::BeginDisabled(gameSpace);
            if(session.menu) drawMenu(game,session,interface,window.handle);
            else drawTable(game,commands,session,feedback,interface,art,window.handle,timeline,settings.animate);
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
            if(freeTable && io.MouseWheel!=0) settings.distance=std::clamp(settings.distance-io.MouseWheel*.5f,gfx::minZoom,gfx::maxZoom);
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
            session.updateBustReaction(0,timeline.visible());
            drawGameplayShading(session,timeline);
            renderer.render(makeScene(timeline,sceneTime,session.menu,session.bustReaction),settings,width,height);
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
                const std::string suffix=height<800?"-small":"-large";
                if(name!="end" || session.gameOverElapsed>=.8f) capture(name);
                if(name=="end") {
                    if(session.gameOverElapsed>=.20f && session.gameOverElapsed<.40f) capture("end-fade-early"+suffix);
                    if(session.gameOverElapsed>=.50f && session.gameOverElapsed<.70f) capture("end-fade-late"+suffix);
                    if(session.gameOverElapsed>=.8f) capture("end"+suffix);
                }
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
                if(game.state().phase==bj::Phase::Shop) {
                    for(auto relic:game.state().offers) {
                        if(relicInfo(relic).rarity==bj::Rarity::Legendary) capture("shop-legendary");
                        if(relicInfo(relic).rarity==bj::Rarity::Rare) capture("shop-rare");
                    }
                }
                if(session.collection && session.collectionFilter) capture("collection-filter");
                if(!session.menu && game.state().phase!=bj::Phase::Shop && !session.history.records().empty())
                    capture("journal-filled");
                if(!session.menu && game.state().phase!=bj::Phase::Shop && session.history.records().size()>=5)
                    capture("journal-history");
                const auto& visible=timeline.visible();
                const auto hand = bj::evaluateHand(visible.player);
                if(timeline.rewardStep()>0 && !visible.score.appliedEffects.empty())
                    capture("applied-relics-"+std::to_string(timeline.rewardStep()));
                const float effectDuration=.5f+(.07f-.5f)*std::clamp(session.animationSpeed,0.0f,1.0f);
                if(!session.menu && !session.settings && !session.rules && timeline.busy() &&
                    session.relicReactionStep>0 && session.relicReactionElapsed>=effectDuration*.25f)
                    capture("relic-reaction-"+std::to_string(session.relicReactionStep)+suffix);
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
