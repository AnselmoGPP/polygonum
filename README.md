# Polygonum (Vulkan renderer)

<br>![Khronos Vulkan logo](https://raw.githubusercontent.com/AnselmoGPP/VulkRend/master/files/Vulkan_logo.png)

**Polygonum** is a graphics engine, as C++ library, that makes it much easier to render computer graphics with [Vulkan®](https://www.khronos.org/vulkan/).

## Table of Contents
+ [Installation](#installation)
+ [How to use](#how-to-use)
    + [General system](#general-system)
    + [Second thread](#second-thread)
+ [Links](#links)

## Installation

<h4>Polygonum offers multiple functionalities:</h4>

- Render any primitive: Points, Lines, Triangles
- 2D and 3D rendering
- Load models (raw data or OBJ files)
- Load textures to pool (any model can use any texture from the pool)
- Define Vertex and Fragment shaders
- Multiple renderings of the same object in the scene (modifiable at any time)
- Load/delete models at any time
- Load/delete textures at any time
- Parallel thread for loading/deleting models and textures (avoids bottlenecks at the render loop)
- Camera system (navigate through the scene)
- Input system
- Multiple layers (Painter's algorithm)
- Define content of the vertex UBO and the fragment UBO
- Allows transparencies (alpha channel)

**Disclaimer: The following content is outdated, but it will updated soon. This is a work in progress.**

<h4>Main project content:</h4>

- _**projects:**_ Contains Polygonum and some example projects.
  - _**Renderer:**_ Polygonum headers and source files.
  - _**shaders**_ Shaders used (vertex & fragment).
    - **environment:** Creates and configures the core Vulkan environment.
    - **renderer:** Uses the Vulkan environment for rendering the models provided by the user.
    - **models:** The user loads his models to Polygonum through the ModelData class.
    - **input:** Manages user input (keyboard, mouse...) and delivers it to Polygonum.
    - **camera:** Camera system.
    - **timer:** Time data.
    - **data:** Bonus functions (model matrix computation...).
    - **main:** Examples of how to use Polygonum.
- _**extern:**_ Dependencies (GLFW, GLM, stb_image, tinyobjloader...).
- _**files:**_ Scripts and images.
- _**models:**_ Models for loading in our projects.
- _**textures:**_ Images used as textures in our projects.

<br>![Vulkan window](https://raw.githubusercontent.com/AnselmoGPP/VulkRend/master/files/window_1.png)

## How to use

Start by creating a `Renderer` object: Pass a callback of the form `void callback(Renderer& r)` as an argument. This callback will be run each frame. It can be used by the user for updating the Uniform Buffer Objects (model matrices, etc.), loading new models, deleting already loaded models, or modifying the number of renderings for each model. All this actions can be performed inside or outside the callback, but always after the creation of the `Renderer` object.

```
Renderer app(callback);
```

Loading a model: There are different ways of loading a model.

- Specify the number of renders and the paths for the model, texture and shaders (vertex and fragment):

```
modelIterator modelIter = app.newModel( 
    3,
    "../models/model.obj",
    "../models/texture.png",
    "../shadrs/vertexShader.spv",
    "../shadrs/fragmentShader.spv" );
```

- Specify the number of renders, a vector of vertex data (vector<Vertex>), a vector of indices (vector<uint32_t>), and the paths for the texture and shaders (vertex and fragment):

```
modelIterator modelIter = app.newModel( 
    3,
    vertex,
    indices,
    "../models/texture.png",
    "../shadrs/vertexShader.spv",
    "../shadrs/fragmentShader.spv" );
```

Deleting a model: 

```
r.deleteModel(modelIter);
```

Modifying the number of renders of a model:

```
r.setRenders(modelIter, 5);
```

For efficiency, it is good practice to load a model by specifying the maximum number of renderings you will create from it, and during rendering you can modify the number of renders with `addRender()`. Why? Because the internal buffer of UBOs of that model needs to be destroyed and created again each time a bigger buffer requires allocation.

For start running the render loop, call `run()`:

```
app.run();
```

## Dependencies

<h4>Dependencies of these projects:</h4>

- **GLFW** (Window system and inputs) ([Link](https://github.com/glfw/glfw))
- **GLM** (Mathematics library) ([Link](https://github.com/g-truc/glm))
- **stb_image** (Image loader) ([Link](https://github.com/nothings/stb))
- **tinyobjloader** (Load vertices and faces from an OBJ file) ([Link](https://github.com/tinyobjloader/tinyobjloader))
- **bullet3** (Physics) ([Link](https://github.com/bulletphysics/bullet3))
- **Vulkan SDK** ([Link](https://vulkan.lunarg.com/sdk/home)) (Set of repositories useful for Vulkan development) (installed in platform-specfic directories)
  - Vulkan loader (Khronos)
  - Vulkan validation layer (Khronos)
  - Vulkan extension layer (Khronos)
  - Vulkan tools (Khronos)
  - Vulkan tools (LunarG)
  - gfxreconstruct (LunarG)
  - glslang (Shader compiler to SPIR-V) (Khronos)
  - shaderc (C++ API wrapper around glslang) (Google)
  - SPIRV-Tools (Khronos)
  - SPIRV-Cross (Khronos)
  - SPIRV-Reflect (Khronos)

## Building the project

<h4>Steps for building this project:</h4>

The following includes the basics for setting up Vulkan and this project. For more details about setting up Vulkan, check [Setting up Vulkan](https://sciencesoftcode.wordpress.com/2021/03/09/setting-up-vulkan/).

### Ubuntu

- Update your GPU's drivers
- Get:
    1. Compiler that supports C++17
    2. Make
    3. CMake
- Install Vulkan SDK
    1. [Download](https://vulkan.lunarg.com/sdk/home) tarball in `extern\`.
    2. Run script `./files/install_vulkansdk` (modify `pathVulkanSDK` variable if necessary)
- Build project using the scripts:
    1. `sudo ./files/Ubuntu/1_build_dependencies_ubuntu`
    2. `./files/Ubuntu/2_build_project_ubuntu`

### Windows

- Update your GPU's drivers
- Install Vulkan SDK
    1. [Download](https://vulkan.lunarg.com/sdk/home) installer wherever 
    2. Execute installer
- Get:
    1. MVS
    2. CMake
- Build project using the scripts:
    1. `1_build_dependencies_Win.bat`
    2. `2_build_project_Win.bat`
- Compile project with MVS (Set as startup project & Release mode) (first glfw and glm_static, then the Vulkan projects)

## Adding a new project

  1. Copy some project from _/project_ and paste it there
  2. Include it in /project/CMakeLists.txt
  3. Modify project name in copiedProject/CMakeLists.txt
  4. Modify in-code shaders paths, if required

## Technical details

### General system

The Renderer class manages the ModelData objects. Both require a VulkanEnvironment object (the same one).

- **VulkanEnvironment:** Contains the common elements required by Renderer and ModelData (GLFWwindow, VkInstance, VkSurfaceKHR, VkPhysicalDevice, VkDevice, VkQueues, VkSwapchainKHR, swapchain VkImages, swapchain VkFramebuffer, VkRenderPass, VkCommandPool, multisampled color buffer (VkImage), depth buffer (VkImage), ...).
- **ModelData:** Contains the data about some mesh (model), a reference to the VkEnvironment object used, VkPipeline, texture VkImage, texture VkSampler, vector of vertex, vector of indices, vertex VkBuffer, index VkBuffer, uniform VkBuffer, VkDescriptorPool, VkDescriptorSet, etc. The mesh data includes vertex (vertices, color, texture coordinates), indices, and paths to shaders and model data (vertices, color, texture coordinates, indices).
- **Renderer:** Contains all the ModelData objects, and the VkEnvironment object used. Also contains the VkCommandBuffer, Input, TimerSet, a second thread, some lists for pending tasks (load model, delete model, set renders for a model), semaphores, etc. 

### Second thread

When the user 
- loads a model (newModel), a ModelData object is partially constructed and stored in a list of "models pending full construction".
- deletes a model (deleteModel), it is annotated for deletion in a list of "models pending deletion".
- changes the number of renders of some model (setRenders), it is annotated for modification in a list of "models pending changing number of renders".

A secondary thread starts running in parallel just after the creation of a Renderer object. Such thread is continuously looking into these 3 lists. If a pending task is found, it is executed in this thread (so, it's done in parallel): fully construction of a ModelData, destroying a ModelData, or modifying the number of renders of some model. All these operations require modifying the command buffer. Different semaphores control access to different lists, the command buffer, and some common parts of code.

### Others

Models with 0 renders have a descriptor set (like every model) but, since it is supposed to render nothing, the commmand for rendering it is not passed to the command buffer.

## Links

- [Setting up Vulkan](https://sciencesoftcode.wordpress.com/2021/03/09/setting-up-vulkan/)
- [Vulkan tutorials](https://sciencesoftcode.wordpress.com/2019/04/08/vulkan-tutorials/)
- [Vulkan notes](https://sciencesoftcode.wordpress.com/2021/11/08/vulkan-notes/)
- [Vulkan SDK (getting started)](https://vulkan.lunarg.com/doc/sdk/1.2.170.0/linux/getting_started.html)
- [Vulkan tutorial](https://vulkan-tutorial.com/)
- [Diagrams](https://github.com/David-DiGioia/vulkan-diagrams)
