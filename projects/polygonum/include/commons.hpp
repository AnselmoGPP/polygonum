#ifndef COMMONS_HPP
#define COMMONS_HPP

#include <list>

//#include <vulkan/vulkan.h>		// From LunarG SDK. Used for off-screen rendering
//define GLFW_INCLUDE_VULKAN		// Makes GLFW load the Vulkan header with it
//#include "GLFW/glfw3.h"

#include "environment.hpp"

// Debugging macros ----------

//#define DEBUG_ENV_INFO			// Basic info
//#define DEBUG_ENV_CORE			// Standards: NDEBUG, _DEBUG

//#define DEBUG_RENDERER
//#define DEBUG_COMMANDBUFFERS
//#define DEBUG_RENDERLOOP
//#define DEBUG_WORKER

//#define DEBUG_MODELS

//#define DEBUG_RESOURCES
//#define DEBUG_IMPORT

//#define DEBUG_ECS


// Functions ----------

//extern std::vector< std::function<glm::mat4(float)> > room_MM;	// Store callbacks of type 'glm::mat4 callb(float a)'. Requires <functional> library

/// Read all of the bytes from the specified file and return them in a byte array managed by a std::vector.
void readFile(const char* filename, std::vector<char>& destination);
void readFile(const char* filename, std::string& destination);

/// Copy a C-style string in destination from source. Used in ModelData and Texture. Memory is allocated in "destination", remember to delete it when no needed anymore. 
void copyCString(const char*& destination, const char* source);


#endif