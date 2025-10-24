#ifndef UBO_HPP
#define UBO_HPP

#include "commons.hpp"

// Forward declarations ----------

class VulkanCore;
class SwapChain;
class Renderer;

// Prototypes ----------

struct LightSet;
struct LightPosDir;
struct LightProps;

struct Material;
struct UBO;


// Definitions ----------

/*
	UBO memory organization in the GPU: 
		- The UBO has to be aligned with "minUBOffsetAlignment" bytes.
		- The variables or members of structs that you pass to the shader have to be aligned with 16 bytes (but variables or struct members created inside the shader doesn't).
		- Due to the 16-bytes alignment requirement, you should pass variables that fit 16 bytes (example: vec4, float[4], int[4]...) or fit your variables in packages of 16 bytes (example: float + int + vec2).

	UBO:
		|--------------------------------minUBOffsetAlignment(256)-----------------------------|
		|---------16---------||---------16---------||---------16---------||---------16---------|
	Data passed:
		|----------------------------my struct---------------------------||--int--|
		|-float-|             |----vec3----|        |--------vec4--------|
*/

namespace sizes {
	extern size_t UniformAlignment;	// Alignment required for each uniform in the UBO (usually, 16 bytes).
	extern size_t vec2;
	extern size_t vec3;
	extern size_t vec4;
	extern size_t ivec4;
	extern size_t mat4;
	//extern size_t materialSize = sizeof(Material);
	//extern size_t lightSize;
};

struct Light
{
	void turnOff();
	void setDirectional(glm::vec3 direction, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular);
	void setPoint(glm::vec3 position, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float constant, float linear, float quadratic);
	void setSpot(glm::vec3 position, glm::vec3 direction, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float constant, float linear, float quadratic, float cutOff, float outerCutOff);

	alignas(16) int type;				//!< 0: no light, 1: directional, 2: point, 3: spot

	alignas(16) glm::vec3 position;
	alignas(16) glm::vec3 direction;	//!< Direction FROM the light source

	alignas(16) glm::vec3 ambient;
	alignas(16) glm::vec3 diffuse;
	alignas(16) glm::vec3 specular;

	alignas(16) glm::vec3 degree;		//!< vec3( constant, linear, quadratic )
	alignas(16) glm::vec2 cutOff;		//!< vec2( cutOff, outerCutOff )
};

/**
	@struct LightSet
	@brief Data structure for light. The LightPosDir is passed to vertex shader, and LightProps to fragment shader.

	Maybe their members should be 16-bytes aligned (alignas(16)).
	Usual light values:
	<ul>
		<li>Ambient: Low value</li>
		<li>Diffuse: Exact color of the light</li>
		<li>Specular: Full intensity is vec3(1.0)</li>
	</ul>
*/
struct LightSet
{
	LightSet(size_t numLights, size_t numActiveLights);
	~LightSet();

	void turnOff(size_t index);
	void addDirectional(size_t index, glm::vec3 direction, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular);
	void addPoint(size_t index, glm::vec3 position, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float constant, float linear, float quadratic);
	void addSpot(size_t index, glm::vec3 position, glm::vec3 direction, glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float constant, float linear, float quadratic, float cutOff, float outerCutOff);
	void printLights() const;

	Light* set;				//!< Set of lights

	const size_t bytesSize;
	const size_t numLights;
	size_t numActiveLights;
};

/**
	@struct Material
	@brief Data structure for a material. Passed to shader as UBO. No textures, just values for Diffuse, Specular & Shininess.

	Basic values:
	<ul>
		<li>Ambient (not included): Object color</li>
		<li>Diffuse (albedo): Object color</li>
		<li>Specular: Specular map</li>
		<li>Shininess: Object shininess</li>
	</ul>
	Examples: http://devernay.free.fr/cours/opengl/materials.html
*/
struct Material
{
	Material(glm::vec3& diffuse, glm::vec3& specular, float shininess);

	// alignas(16) vec3 ambient;		// Already controlled with the light
	alignas(16) glm::vec3 diffuse;		// or sampler2D diffuseT;
	alignas(16) glm::vec3 specular;		// or sampler2D specularT;
	alignas(16) float shininess;
};

struct BindingInfo
{
	virtual ~BindingInfo();

	template<typename T, typename... Args>
	static std::shared_ptr<T> create(Args&&... args);
};

template<typename T, typename... Args>
std::shared_ptr<T> BindingInfo::create(Args&&... args)
{
	if (std::is_base_of_v<BindingInfo, T> == false)
		throw std::runtime_error("Not valid object type.");

	return std::make_shared<T>(std::forward<Args>(args)...);
}

/// A UBO array is a type of Binding. A Binding is an array of descriptors. Each descriptor has attributes. There're different types of descriptors (UBO, sampler...).
struct UbosArrayInfo : BindingInfo
{
	UbosArrayInfo(size_t maxNumUbos, size_t numActiveUbos, size_t uboSize, const std::vector<std::string>& glslLines = {});
	UbosArrayInfo();

	size_t maxNumUbos;   //!< Number of descriptors
	size_t numActiveUbos;   //!< Descriptors used
	size_t uboSize;   //!< Descriptor size

	std::vector<std::string> glslLines;   //!< (Optional) Used in ShaderCreator
};

/// Container for a binding of uniform buffers, which is an array of uniform buffer descriptors (UBOs). Multiple UBOs are useful for instance rendering.
struct UBOsArray
{
private:
	VulkanCore* c;
	SwapChain* swapChain;

public:
	UBOsArray(Renderer* renderer, const UbosArrayInfo& bindingInfo);
	UBOsArray();
	~UBOsArray() = default;
	UBOsArray(UBOsArray&& other) noexcept;   //!< Move constructor: Tansfers resources of a temporary object (rvalue) to another object.
	UBOsArray& operator=(UBOsArray&& other) noexcept;   //!< Move assignment operator: Transfers resources from one object to another existing object.

	uint32_t					maxNumUbos;			//!< Max. possible number of descriptors. This is fixed because it's fixed in the shader.
	uint32_t					numActiveUbos;		//!< Number of descriptors used (must be <= maxDescriptors). 
	VkDeviceSize				uboSize;			//!< Size (bytes) of each aligned descriptor (example: 4) (at least, minUBOffsetAlignment)
	size_t						totalBytes;			//!< Size (bytes) of the array of UBOs (example: 12)

	std::vector<uint8_t>		binding;			//!< Array of UBOs will be passed to vertex shader (MVP, M for normals, light...). Its attributes are aligned to 16-byte boundary.
	std::vector<VkBuffer>		bindingBuffers;		//!< Opaque handle to a buffer object (here, a binding). One for each swap chain image.
	std::vector<VkDeviceMemory>	bindingMemories;	//!< Opaque handle to a device memory object (here, memory for the binding). One for each swap chain image.

	std::vector<std::string> glslLines;   //!< (Optional) Used in ShaderCreator

	bool setNumActiveUbos(size_t count);			//!< Set the value of activeUBOs. Returns false if > maxUBOcount;
	uint8_t* getUboPtr(size_t uboIndex);
	void createBinding();								//!< Create uniform buffers (type of descriptors that can be bound) (VkBuffer & VkDeviceMemory), one for each swap chain image. At least one is created (if count == 0, a buffer of size "range" is created).
	void destroyBinding();				//!< Destroy the uniform buffers (VkBuffer) and their memories (VkDeviceMemory).
};

/// Model-View-Projection matrix as a UBO (Uniform buffer object) (https://www.opengl-tutorial.org/beginners-tutorials/tutorial-3-matrices/)
/*
struct UBO_MVP {
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};
 
struct UBO_MVPN {
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
	alignas(16) glm::mat3 normalMatrix;
};
*/

#endif