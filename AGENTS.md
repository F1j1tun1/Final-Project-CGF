# Project instructions

This is a student OpenGL project: a playable starter prototype, not a finished game. Read `README.md`, `docs/ARCHITECTURE.md`, and `docs/HANDOFF_VSCODE.md` before changes. The course explicitly requires a modern shader-based OpenGL pipeline. Keep C++17, OpenGL 3.3 Core, CMake 3.24+, and vendored dependencies; do not replace the project with a game engine or a website.

The canonical team repository is https://github.com/F1j1tun1/Final-Project-CGF. Work in its checkout, not the original prototype output folder. Read `CONTRIBUTING.md` for collaboration. Preserve other participants' changes and coordinate shared files; use focused feature branches and pull requests for subsequent changes. Do not force-push or change repository access/branch-protection settings without a user request.

## Module boundaries

- `src/game/`: pure C++ rules, seeded randomness, run state, scoring, and shop. No GLFW, OpenGL, ImGui, file I/O, wall-clock dependencies, or visual animation state here.
- `src/graphics/`: meshes, textures, shaders, camera, lighting, and rendering. Consume visual scene data; do not decide game outcomes or rewards.
- `src/main.cpp`: window lifetime, input, UI, commands, and conversion of the game snapshot into a visual scene. Extract small presentation modules when needed; avoid a framework rewrite.
- Own GPU resources with RAII and destroy them while the OpenGL context is alive. Preserve the model/view/projection and normal-matrix pipeline.

## Development and verification

- Keep same-seed + same-actions runs deterministic. Blocked commands must return false without mutating state.
- Add meaningful tests for changed rules or phase behavior; keep low-impact visual edits free of implementation-mirroring tests.
- Run `cmake --preset core-only`, `cmake --build --preset core-only --parallel`, and `ctest --preset core-only` for rule changes.
- For graphics/integration changes, also build/test the `desktop` preset and run the executable with `--smoke` when a working GPU session is available. Inspect a screenshot or visible run for layout and rendering.
- Report what actually ran and any unverified platform; rule tests alone do not establish graphics correctness.
- Keep asset lookup independent of the terminal's working directory. Preserve the build's adjacent `assets/` copy.
- Preserve third-party source notices and licenses. Avoid editing vendored libraries unless required by a concrete compatibility fix.
- Keep docs consistent with final behavior. Documentation is Russian; source identifiers and the current UI are English.

The roadmap includes event-based card deal/flip animation, audio and balance. Follow the user's current requested task; do not start unrelated roadmap work automatically.
