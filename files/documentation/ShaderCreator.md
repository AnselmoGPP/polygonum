# ShaderCreator

## Table of content

+ [Shaders](#shaders)
+ [Subpasses](#subpasses)
+ [Vertex type](#vertex-type)
+ [Automating shaders](#automating-shaders)
+ [ShaderCreator](#shadercreator)

## Shaders

There're different shaders (VS, FS…). Their contents vary depending on the **Subpass** they belong to. Most important parts of a shader:

- **Bindings**: Data passed from CPU or loaded textures.
  - **Buffers**
  - **Textures**
- **Inputs**: Data passed from previous stage (like vertex data)
- **Outputs**: Data passed to the next stage
- **Main function**: Uses bindings and inputs to generate the outputs

Shader examples:

- **Vertex shader** (geometry subpass):

```
#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

layout(set = 0, binding = 0) uniform GlobalBuffer { ... } gBuf;
layout(set = 0, binding = 1) buffer LocalBuffer { ... } lBuf;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;

void main()
{
    outWorldPos = (lBuf.instance[i].model * vec4(inPos, 1.0)).xyz;
    outNormal = mat3(lBuf.instance[i].normalMat) * inNormal;
    outUV = inUV;
    gl_Position = gBuf.PM * gBuf.VM * vec4(outWorldPos, 1.0);
}
```

- **Fragment shader** (geometry subpass):

```
#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

layout(early_fragment_tests) in;

layout(set = 0, binding = 2) uniform GlobalBuffer { ... } gBuf;
layout(set = 0, binding = 3) uniform sampler2D tex[4];

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outPos;
layout(location = 1) out vec4 outAlbedo;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec4 outSpecRoug;

void main()
{
    outPos = vec4(inWorldPos, 1.f);
	outAlbedo = vec4(texture(tex[0], inUV).xyz, 1.f);
	outNormal = vec4(normalize(inNormal), 1.0);
	outSpecRoug = vec4(vec3(0.f, 0.f, 0.f), 0.f);
}

```

## Subpasses

**Render pipeline**: It's made of **Render passes**, each one made of one or more subpasses. Common passes are:

- **Geometry pass** (GP): Receives vertex data. Outputs fragment's lighting parameters.
- **Lighting pass** (LP): Receives vertex data (quad) and textures with pixel's lighting parameters. Outputs fragment's final color after lighting processing.
- **Forward pass** (FP): Receives vertex data. Outputs fragment's final color after lighting processing.
- **Post-processing pass** (PP): Receives vertex data (quad) with rendering. Outputs quad with processed rendering.

**Deferred shading** (DS): Combination of two subpasses: Geometry and Lighting. This is more efficient than a Forward pass because DS only processes lighting for all pixels, while FP does it for all framents.

All subpasses have the same vertex shader. It receives vertex data (of a quad or any other object/s) and at least passes `gl_Position` to the fragment shader.

## Vertex type

The mesh is made of vertexes. Vertexes are made of vertex attributes, like Position, Normal, UVs, Tangent, Color.

## Automating shaders

We want to automate shader creation (VS & FS) depending on these elements:

- **Subpass**: Geometry, Lighting, Forward, Postprocessing
  - Determines: inputs, outputs, main
- **Vertex type**: Position, Normal, UVs, Tangent
  - Determines: VS inputs, main (color/lighting)
- **Bindings**: Descriptor set, Binding, Descriptors
  - Determines: bindings (buffers, textures), main (color/lighting)

Rules and assumptions:

- Vertex data always contains Position as vertex attribute.
- Normal maps can only be used if vertex data contains Tangent as vertex attribute.
- By default, albedo is (0,1,0), specularity is (0,0,0), and roughness is 0.

`ShaderCreator`'s constructor adds:

- Header lines: Version, extensions, shader stage, includes, …
- Bindings: SSBOs, UBOs, textures
- Shader content: Input, output, `main` operations

### Vertex shader (Geometry, Forward)

- All vertex attributes add a line to VS's `intput`, `output`, `main_begin`, `main_end`, and to FS's `input`.
  - Exception: `vaFix` is only added to `input`.
- `main` computations:
  - `vaPos` (always) → `gl_Position` and `outPos` (world_position). Requires `LocalBuffer` and `GlobalBuffer`.
  - `vaNorm` → `outNormal`. Requires `LocalBuffer` (transforms normal using `normalMat`).
  - `vaTan` → `outTB` (tangent & bitangent). Requires `vaNorm` (otherwise, `outTB` is not computed).
  - Other attributes (`vaCol`, `vaUV`…) → Output directly
  - `vaFixes` → Not output
- Bindings assumed:

```
struct InstanceData   // in vertexTools.vert
{
	mat4 model;
	mat4 normalMat;
};

layout(set = 0, binding = 0) uniform GlobalBuffer {   // UBO
        mat4 view;
        mat4 proj;
        vec4 camPos_t;
} gBuf;

layout(set = 0, binding = 1) buffer LocalBuffer {   // SSBO
        InstanceData ins[];
} lBuf;
```

### Fragment shader (Forward)

- `main` computations:
  - Output: `outColor`. Requires `textures`, `inUV`, `GlobalBuffer`, `inTB` (normals).
    - No `vaUv` → Default albedo, specularity, and roughness. Uses vertex normal only (no normal map).
	  - Has `vaColor` or `vaCol4` → Use vertex color as albedo.
	- No `vaTan` → Uses vertex normal only
	- No `vaNorm` → Exception error (vertex normals required for lighting)

### Fragment shader (Geometry)

- Required outputs: `outPos`, `outAlbedo`, `outNormal`, `outSpecRoug`
  - Each value is taken from the appropriate texture, if it exists (`tAlb`, `tSpec`, `tRoug`, `tSpecroug`, `tNorm`).
  - No texture → Default values
  - If `tNorm` and `vaTan` exist, both are used to compute `outNormal`

- Bindings assumed:

```
layout(set = 0, binding = 2) uniform GlobalBuffer {
        vec4 camPos_t;
        Light light[NUMLIGHTS];
} gBuf;

layout(set = 0, binding = 3) uniform sampler2D tex[4];  // Albedo > Spec > Rough > Normal
```

### Vertex & Fragment shader (Lighting, Postprocessing)

- A quad is rendered
- [`inPos` (NDC), `inUVs`] → Vertex shader → [`gl_Position`, `outUV`]
- [`inUV`] → Fragment shader → [`outColor`]

## ShaderCreator

**`class ShaderCreator`** is a programmatic GLSL shader generator. Instead of writing vertex/fragment shaders by hand, you construct them in C++ by composing strings. How it works:

1. You construct a `ShaderCreator(rpType, vertexType, bindings)`. Based on the render pass type (`geometry`, `lighting`, `forward`, `postprocessing`), it fills a `ShaderCode` struct with different `std::vector<std::string>` (typically):

- `header`, `includes`, `flags`
- `structs`
- `bind_globalBuffers`, `bind_localBuffers`, `bind_textures`
- `input`, `output`
- `globals`
- `main_begin`, `main_processing`, `main_end`
- `others`

2. getShader(0) and getShader(1) assemble the final GLSL by concatenating all those pieces in order:
header → includes → flags → structs → bindings → inputs → outputs → globals → main()
The bindings section even generates layout(set=0, binding=N) declarations and wraps UBO/SSBO members into structs automatically.
3. For geometry and forward passes, setVS_general() looks at each vertex attribute type (vaPos, vaNorm, vaTan, vaUv, etc.) and generates the corresponding shader I/O lines and transform code.
4. Usage: In example_2 and example_3, the pipeline creates shaders without writing a single GLSL file — it creates ShaderCreator objects, optionally calls replaceMainBegin()/replaceMainEnd() to customize behavior, then passes them through ShaderLoader to compile via shaderc.
5. The alternative is SL_fromFile, which loads a .vert/.frag file from disk. ShaderCreator is the alternative for auto-generation.
In short: it's a C++-embedded GLSL code generator that maps vertex types and bindings to shader code at runtime, avoiding manual shader authoring for the pipeline's standard passes

`ShaderCreator` needs 3 parameters:

- **Render pass type** (`rpType`): To generate inputs, outputs, and `main`
- **Vertex type** (`vertexType`): To generate inputs and `main`
- **Bindings** (`bindings`): To generate bindings.









Shader examples:

- Vertex shader (Geometry pass):

```
// Vertex shader - Geometry
#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

#include "..\..\extern\polygonum\resources\shaders\vertexTools.vert"

layout(set = 0, binding = 0) uniform GlobalBuffer {
        mat4 view;
        mat4 proj;
        vec4 camPos_t;
} gBuf;

layout(set = 0, binding = 1) buffer LocalBuffer {
        InstanceData ins[];
} lBuf;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;

int i = gl_InstanceIndex;

void main()
{
        vec3 worldPos = (lBuf.ins[i].model * vec4(inPos, 1.0)).xyz;
        vec4 clipPos = gBuf.proj * gBuf.view * vec4(worldPos, 1.0);
        vec3 normal = mat3(lBuf.ins[i].normalMat) * inNormal;
        vec2 uv = inUV;

        gl_Position = clipPos;
        outPos = worldPos;
        outNormal = normal;
        outUV = uv;
}
```

- Fragment shader (Geometry pass):

```
// Fragment shader - Geometry
#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

#include "..\..\extern\polygonum\resources\shaders\fragTools.vert"

layout(early_fragment_tests) in;

layout(set = 0, binding = 2) uniform GlobalBuffer {
        vec4 camPos_t;
        Light light[NUMLIGHTS];
} gBuf;

layout(set = 0, binding = 3) uniform sampler2D tex[4];

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outPos;
layout(location = 1) out vec4 outAlbedo;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec4 outSpecRoug;

void main()
{
        vec3 worldPos = inPos;
        vec4 albedo = vec4(texture(tex[0], inUV).xyz, 1.f);
        vec3 specularity = vec3(0.f, 0.f, 0.f);
        float roughness = 0.f;
        vec3 normal = inNormal;

        outPos = vec4(worldPos, 1.f);
        outAlbedo = albedo;
        outSpecRoug = vec4(specularity, roughness);
        outNormal = vec4(normalize(normal), 1.0);
}
```