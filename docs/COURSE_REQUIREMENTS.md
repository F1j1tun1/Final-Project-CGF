# CGF course requirements

The assignment calls for interactive graphics with OpenGL and a modern shader pipeline. Jester Jack uses C++17, OpenGL 3.3 Core, and CMake 3.24+.

## Graphics techniques

| Requirement | Implementation and demonstration |
| --- | --- |
| Real-time interactive graphics | GLFW/OpenGL loop, mouse and keyboard input |
| Modern shader-based pipeline | VAO/VBO/EBO, custom GLSL vertex and fragment shaders |
| Scene setup | 3D table, cards, chips, casino, camera, and lighting |
| Transformation pipeline | Model/view/projection and normal matrices through GLM |
| Lighting/shading | Blinn–Phong: ambient, diffuse, specular |
| Texturing or equivalent effect | Procedural images, GL_TEXTURE_2D, UVs, and shader sampling |

ImGui supplements the custom 3D scene. During the presentation, show object transformations, lighting, textures, and input response, and explain how the C++ data connects to GLSL.

## Submission materials

- A repository that builds on the instructor's machine: source code, dependencies, licenses, and build instructions.
- A live demonstration or a 3–6 minute video.
- A written report using the course template and slides covering the task, architecture, graphics techniques, verification results, sources, and each contributor's work.

## Milestones

| Course period | Materials |
| --- | --- |
| Week 5 / Midterm | Proposal; prototype with a render loop and one feature; Introduction, Background, Project Description, Methodology |
| Week 7 | Feature-complete beta: lighting, textures, and interaction |
| Finals week | Demonstration, report, and slides |

In the available rubric excerpt, Technical depth & correctness is worth 35%. Technical sources and lecture pages are listed in [LECTURE_ALIGNMENT.md](LECTURE_ALIGNMENT.md); the project architecture is described in [ARCHITECTURE.md](ARCHITECTURE.md).
