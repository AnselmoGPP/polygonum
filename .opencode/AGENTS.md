# Polygonum Codebase Knowledge

## Overview
Polygonum is a low-level **Vulkan-based 3D rendering engine** in **C++17**, built as a static library. Author: Anselmo Gracia. MIT License.

## Project structure
```
polygonum/
  projects/
    polygonum/        # Core library
      include/polygonum/   # Public headers (14 .hpp files)
      src/                 # Implementations (15 .cpp files)
      shaders/             # Shader compilation scripts
    example_1/         # Basic triangle + textured background
    example_2/         # 3D rotating cube, deferred shading, skybox, camera
    example_3/         # 3D rock model, deferred shading + skybox
  extern/
    glfw/glfw-3.3.2/
    assimp/
    stb/
  resources/
    shaders/           # Built-in render pipeline shaders (GLSL)
    textures/
  _BUILD/              # Build output (gitignored)
  files/               # Build scripts, docs, reference
```

## Dependencies
- Vulkan SDK 1.3.280.0 (hardcoded path: `C:/VulkanSDK/1.3.280.0/`)
- GLFW 3.3.2 (bundled in `extern/glfw/`)
- GLM 0.9.9.5 (built via GLFW's CMake)
- Assimp (bundled full source in `extern/assimp/`)
- stb_image.h (bundled in `extern/stb/`)
- shaderc (from Vulkan SDK, runtime GLSL->SPIR-V)
- zlib (bundled as Assimp dependency)

## Architecture

### Main facade: Renderer
`projects/polygonum/include/polygonum/renderer.hpp` / `src/renderer.cpp`
- Manages render loop, models, textures, shaders, worker thread, command buffers, sync objects
- Constructor: `Renderer(updateCallback, io, globalUBO_vs_info, globalUBO_fs_info)`
- Entry: `renderer.renderLoop()`

### Key modules (in dependency order)
| Module | Header | Responsibility |
|---|---|---|
| commons.hpp | include/ | Centralized Vulkan/GLFW/GLM/Assimp/shaderc includes |
| toolkit.hpp | include/ | MVP matrices, quaternions, frustum culling, bounding shapes, math utilities, sorting, geometry |
| input.hpp | include/ | GLFW window, keyboard/mouse input, callbacks, cursor |
| ubo.hpp | include/ | Uniform buffer management, alignment, instance rendering |
| vertex.hpp | include/ | Vertex attribute definitions, GPU buffer containers, vertex loading |
| texture.hpp | include/ | Vulkan texture container, loading from file/buffer |
| bindings.hpp | include/ | Groups UBO bindings and textures for shader stages |
| shader.hpp | include/ | Shader modules, runtime GLSL->SPIR-V, auto-generation, shader modifications |
| environment.hpp | include/ | VulkanCore, SwapChain, RenderPipeline, Commander, Image, RenderPass |
| models.hpp | include/ | ModelData (per-model pipeline/descriptors/buffers), ModelsManager |
| importer.hpp | include/ | Orchestrates loading vertices+shaders+textures for a model |
| physics.hpp | include/ | Particle system, planet gravity, atmosphere rendering |
| ecs.hpp | include/ | Entity-Component-System |

### Render pipeline (RP_DS_PP default)
```
Geometry Pass  -> position, albedo, normal, specularity_roughness (+depth)
Lighting Pass  -> reads G-buffer, outputs lit scene
Forward Pass   -> additional objects (transparency)
Post-Processing -> effects, output to swapchain
```

## Build system
- CMake >= 3.12, Visual Studio 17 2022 (x64)
- Build commands in `files/build_project_Win.bat`
- Static library target: `polygonum`
- Debug mode is default (no `-O2`)

## Key patterns
- `PointersManager<T>`: shared_ptr/weak_ptr reference counting for shaders/textures
- `VertexesLoader` ADT: `VL_fromFile` (Assimp) or `VL_fromBuffer` (raw data)
- `ShaderLoader` ADT: `SL_fromFile` or `SL_fromBuffer`
- `SMod`: shader modification system (albedo, specular, wave, dithering, etc.)
- `LoadingWorker`: background thread for async model load/unload
- `Help_RP_DS_PP`: helper creating the default render pass pipeline
- `VAL_LAYERS` macro: conditionally enables Vulkan validation layers
- Resource paths are relative to `resources/` directory
