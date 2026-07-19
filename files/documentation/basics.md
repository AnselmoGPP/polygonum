# Basics

## Table of content

+ [Model](#model)
+ [Animation](#animation)
+ [Shaders](#shaders)

## Model

A **single model** can contain:

- **Vertex data** (always):
  - Set of vertices and associated per-vertex data
  - Main per-vertex data: position, normal, color, UV
  - Used for generating primitives (points, lines, or faces).
- **Indices**:
  - Define the primitives (lines or faces) from the vertices.
- **Shaders**:
  - Transform the vertex data (position and color) and create geometry
  - Vertex shader (**VS**): Transforms vertex positions.
  - Fragment shader (**FS**): Transforms fragment colors.
- **Textures**:
  - Main types: albedo (`a`), normal (`n`), specularity (`s`), roughness (`r`), height (`h`)
- Gives color, and modifies it, to geometry

## Animation

**Render loop**: Loop that renders one frame per iteration.

**Animation**: Objects can be animated by passing data to the shader in each iteration (like `deltaTime`) and use it to transform the object. Steps:

- Load **Vertex data**, **Indices**, **Shaders**, and **Textures** onto GPU.
- In each **render-loop** iteration:
  - Calculate data (delta time, translation, rotations, scaling…)
  - Pass that data to the shaders (as UBOs, SSBOs…), usually as transformation matrices
  - Shaders will use that data, vertex data, and textures to transform vertices and colors

## Shaders

- **VS**: Transforms vertex **position** usually by applying transformation matrices:
  - **MM** (Model matrix): Made of various matrices: Scaling (**SM**), Rotation (**RM**), Translation (**TM**)
  - **VM** (View matrix): Used to "move the camera" (by moving the object)
  - **PM** (Projection matrix): Defines the Field Of View (FOV)
  - MM & VM are often passed to the shader in each iteration. PM usually doesn't need to be updated so frequently.
- **FS**: Gives and transforms fragment colors usually using:
  - **Color**: Vertex color
  - **Normal**: Where the vertex is pointing. Determines how the light affects it.
  - **UV**: Texture coordinates.
  - **Others**: Roughness, specularity…

- Models can be loaded and unloaded in parallel.