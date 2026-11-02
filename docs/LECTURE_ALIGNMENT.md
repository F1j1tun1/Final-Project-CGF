# CGF lectures and implementation

Sources: Computer Graphics Fundamentals course materials, Astana IT University, from `CGF Lectures.zip`. The course materials are by E. Angel and D. Shreiner and published by Addison-Wesley. Page numbers below are 1-based PDF page numbers, not printed slide numbers.

## Topics and pages

| Topic | File and PDF pages | Application |
| --- | --- | --- |
| Shaders, attributes, uniforms | `L-2-2 Computer Graphics Fundamentals.pdf`, 2–3, 7–9 | GLSL compilation/linking, mesh attributes, and uniforms |
| Transformations | `L-4-1_Transformations.pdf`, 23, 45, 48 | Matrix order, model/view/projection, and shader inputs |
| Camera and perspective | `L-5-1_Viewing.pdf`, 3, 11–12, 43, 46 | GLM lookAt, perspective for the table viewport, viewport |
| Lighting | `L-5-2_Shading.pdf`, 14, 29, 42, 58, 66–70 | Per-fragment Blinn–Phong, halfway vector, normalization |
| Textures | `L-8_TextureMapping.pdf`, 24–25, 43–47 | CPU image, GL_TEXTURE_2D, UVs, and sampler |
| Input and window | `L-3-1_Input.pdf`, 14–15, 24, 39 | Events, buttons, keyboard, and window resizing |

## Core profile and historical examples

The lectures include GLUT, glMatrixMode, glEnable(GL_TEXTURE_2D), gl_FragColor, and texture2D from older APIs. The project uses OpenGL 3.3 Core and GLSL 330: custom matrices, vertex attributes, and fragment outputs. The storage format of the course's mat.h differs from GLM; an example that uploads with GL_TRUE cannot be transferred mechanically to GLM's column-major matrices.

## What to demonstrate

- The interactive table, cards, decor, camera, and lighting changes.
- Continuous mesh surfaces for the mask, clothing, gloves, and curtains with normals; the dealer's sleeves update vertices and normals in a buffer, while the hands move with the cards through model matrices.
- Worn green velvet through a UV texture and directional highlight, local candle light and one hanging lamp; contact shadows, trails, and landing rings use blending and depth testing.
- Camera rotation with right-drag, zoom with the scroll wheel, and smooth dealing through time-based transform interpolation.
- Perspective depth in the arches and columns, occasional smooth lamp flicker, and a sedan driving along the menu street; Ambient motion stops decorative cycles without changing game randomness.
- The `projection * view * model * position` formula and normal matrix `transpose(inverse(mat3(model)))`.
- Attribute/uniform inputs, UV coordinates, the illustration atlas, and TGA card-back loading; the checkerboard floor is computed in one surface's shader.
- How HIT/STAND relates to hand totals, busts, and the brief total/suit-change hint; window and render-area resizing.
- Volumetric dust in world coordinates, its lighting and transparent-particle sorting; zero to eight physical chips matching equipped relics, with selection for inspection and separate ability activation.
- The casino entrance transition: buttons fade, the camera approaches and slows down, doors open, and the scene changes during a black frame; skipping the transition and the option without camera movement.
- Build commands and the verification actually performed on the specified hardware.

The course PDFs retain their original authorship and are not included with the game. Cormorant Garamond and Source Sans 3 are distributed under SIL OFL 1.1; authors, sources, and checksums are listed in [assets/fonts/README.md](../assets/fonts/README.md). Submission requirements are in [COURSE_REQUIREMENTS.md](COURSE_REQUIREMENTS.md).
