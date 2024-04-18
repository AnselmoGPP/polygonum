#ifndef VERTEX_HPP
#define VERTEX_HPP

#include <array>

#include <vulkan/vulkan.h>			// From LunarG SDK. Used for off-screen rendering
//#define GLFW_INCLUDE_VULKAN		// Makes GLFW load the Vulkan header with it
#include "GLFW/glfw3.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE			// GLM uses OpenGL depth range [-1.0, 1.0]. This macro forces GLM to use Vulkan range [0.0, 1.0].
#define GLM_ENABLE_EXPERIMENTAL				// Required for using std::hash functions for the GLM types (since gtx folder contains experimental extensions)
#include <glm/glm.hpp>
//#include <glm/gtc/matrix_transform.hpp>	// Generate transformations matrices with glm::rotate (model), glm::lookAt (view), glm::perspective (projection).
#include <glm/gtx/hash.hpp>

/*
	Content:
		- VertexType
		- VertexSet
			- VertexPCT
			- VertexPC
			- VertexPT
			- VertexPTN
*/

/// VertexType defines the characteristics of a vertex. Defines the size and type of attributes the vertex is made of (Position, Color, Texture coordinates, Normals...).
class VertexType
{
public:
	VertexType(std::initializer_list<size_t> attribsSizes, std::initializer_list<VkFormat> attribsFormats);	//!< Constructor. Set the size (bytes) and type of each vertex attribute (Position, Color, Texture coords, Normal, other...).
	VertexType();
	~VertexType();
	VertexType& operator=(const VertexType& obj);				//!< Copy assignment operator overloading. Required for copying a VertexSet object.

	VkVertexInputBindingDescription getBindingDescription() const;						//!< Used for passing the binding number and the vertex stride (usually, vertexSize) to the graphics pipeline.
	std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() const;	//!< Used for passing the format, location and offset of each vertex attribute to the graphics pipeline.

	std::vector<VkFormat> attribsFormats;			//!< Format (VkFormat) of each vertex attribute. E.g.: VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT...
	std::vector<size_t> attribsSizes;				//!< Size of each attribute type. E.g.: 3 * sizeof(float)...
	size_t vertexSize;								//!< Size (bytes) of a vertex object
};


/// VertexSet serves as a container for any object type, similarly to a std::vector, but storing such objects directly in bytes (char array). This allows ModelData objects store different Vertex types in a clean way (otherwise, templates and inheritance would be required, but code would be less clean).
class VertexSet
{
public:
	VertexSet();
	VertexSet(size_t vertexSize);
	//VertexSet(VertexType vertexType, size_t numOfVertex, const void* buffer);
	~VertexSet();
	VertexSet& operator=(const VertexSet& obj);	// Not used
	VertexSet(const VertexSet& obj);			//!< Copy constructor

	size_t vertexSize;

	size_t totalBytes() const;
	size_t size() const;
	char* data() const;
	void push_back(const void* element);
	void reserve(unsigned size);
	void reset(size_t vertexSize, size_t numOfVertex, const void* buffer);	//!< Similar to a copy constructor, but just using its parameters instead of an already existing object.
	void reset(size_t vertexSize);

	// Debugging purposes
	void* getElement(size_t i) const;		//!< Get pointer to element
	void printElement(size_t i) const;		//!< Print vertex floats
	void printAllElements() const;	//!< Print vertex floats of all elements
	size_t getNumVertex() const;

private:
	char* buffer;			// Set of vertex objects stored directly in bytes
	size_t capacity;		// (resizable) Maximum number of vertex objects that fit in buffer
	size_t numVertex;		// Number of vertex objects stored in buffer
};


/// Vertex structure containing Position, Color and Texture coordinates.
struct VertexPCT
{
	VertexPCT() = default;
	VertexPCT(glm::vec3 vertex, glm::vec3 vertexColor, glm::vec2 textureCoordinates);

	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription					getBindingDescription();	///< Describes at which rate to load data from memory throughout the vertices (number of bytes between data entries and whether to move to the next data entry after each vertex or after each instance).
	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();	///< Describe how to extract a vertex attribute from a chunk of vertex data originiating from a binding description. Two attributes here: position and color.
	bool operator==(const VertexPCT& other) const;										///< Overriding of operator ==. Required for doing comparisons in loadModel().
};

/// Hash function for VertexPCT. Implemented by specifying a template specialization for std::hash<T> (https://en.cppreference.com/w/cpp/utility/hash). Required for doing comparisons in loadModel().
template<> struct std::hash<VertexPCT> {
	size_t operator()(VertexPCT const& vertex) const;
};

#endif