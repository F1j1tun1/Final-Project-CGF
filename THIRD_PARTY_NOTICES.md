# Third-party libraries

Dependencies are included as source in `third_party/`; no downloads happen during CMake configuration. Keep the upstream licenses when sharing source or binaries.

| Component | Pinned version | License location |
| --- | --- | --- |
| [GLFW](https://github.com/glfw/glfw/tree/3.4) | 3.4 | `third_party/glfw/LICENSE.md` (zlib/libpng) |
| [GLM](https://github.com/g-truc/glm/tree/1.0.1) | 1.0.1 | `third_party/glm/copying.txt` (MIT or modified MIT) |
| [Dear ImGui](https://github.com/ocornut/imgui/tree/v1.91.9b) | v1.91.9b | `third_party/imgui/LICENSE.txt` (MIT) |
| GLAD loader | Generated header supplied with GLFW 3.4 | `third_party/glfw/deps/glad/gl.h` (generated source) |
| stb_image_write | Header supplied with GLFW 3.4 | License text at the end of `third_party/glfw/deps/stb_image_write.h` (MIT or public domain) |

Downloaded release archive SHA-256 hashes:

```text
glfw  3.4      a133ddc3d3c66143eba9035621db8e0bcf34dba1ee9514a9e23e96afd39fd57a
glm   1.0.1    09c5716296787e1f7fcb87b1cbdbf26814ec1288ed6259ccd30d5d9795809fa5
imgui 1.91.9b  fd37507c8476a6d14cc7c4b352401f31bcbd0f0d995d35390811e968c466f46e
```

The game uses its own procedural meshes and textures; no Balatro artwork, audio or source code is included. Select the license for your team's own code before publicly publishing the repository.
