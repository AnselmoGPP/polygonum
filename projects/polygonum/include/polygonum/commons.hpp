#ifndef COMMONS_HPP
#define COMMONS_HPP

#include "vulkan/vulkan.h"   // From LunarG SDK. Can be used for off-screen rendering

#define GLFW_INCLUDE_VULKAN   // Makes GLFW load the Vulkan header with it
#include "GLFW/glfw3.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // GLM uses OpenGL depth range [-1.0, 1.0]. This macro forces GLM to use Vulkan range [0.0, 1.0].
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"   // Generate transformations matrices with glm::rotate (model), glm::lookAt (view), glm::perspective (projection).
#include "glm/gtc/type_ptr.hpp"
#define GLM_ENABLE_EXPERIMENTAL   // Required for using std::hash functions for the GLM types (since gtx folder contains experimental extensions)
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/hash.hpp>

//#define STB_IMAGE_IMPLEMENTATION   // Import textures
//#include "stb_image.h"

#include "assimp/Importer.hpp"   // Import models (vertices)
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "shaderc/shaderc.hpp"   // Compile shaders (GLSL code to SPIR-V)

//#include "btBulletDynamicsCommon.h"


// Debugging macros ----------

#define DEBUG_ENV_INFO   // Basic info
//#define DEBUG_ENV_CORE   // Standards: NDEBUG, _DEBUG

//#define DEBUG_RENDERER
//#define DEBUG_COMMANDBUFFERS
//#define DEBUG_RENDERLOOP
//#define DEBUG_WORKER
#define DEBUG_REND_PROFILER

//#define DEBUG_MODELS

//#define DEBUG_RESOURCES
//#define DEBUG_IMPORT

//#define DEBUG_ECS


// Typedefs ----------

using key64 = uint64_t;
template<typename T>
using vec = std::vector<T>;
template<typename T>
using vec2 = std::vector<std::vector<T>>;
template<typename T>
using vec3 = std::vector<std::vector<std::vector<T>>>;
template<typename T1, typename T2>
using vecpair = std::vector<std::pair<T1, T2>>;


// Functions ----------

//extern std::vector< std::function<glm::mat4(float)> > room_MM;	// Store callbacks of type 'glm::mat4 callb(float a)'. Requires <functional> library

/// Read all of the bytes from the specified file and return them in a byte array managed by a std::vector.
void readFile(const char* filename, std::vector<char>& destination);
void readFile(const char* filename, std::string& destination);

/// Copy a C-style string in destination from source. Used in ModelData and Texture. Memory is allocated in "destination", remember to delete it when no needed anymore. 
void copyCString(const char*& destination, const char* source);

/*
template <typename k, typename e>
void transfer(std::unordered_map<k, e>& source, std::unordered_map<k, e>& dest, k key)
{
    std::unordered_map<k, e>::node_type node = source.extract(k);

    if (node.empty() == false)
        dest.insert(std::move(node));
};
*/
#endif