#ifndef UBO_HPP
#define UBO_HPP

#include <array>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE			// GLM uses OpenGL depth range [-1.0, 1.0]. This macro forces GLM to use Vulkan range [0.0, 1.0].
#define GLM_ENABLE_EXPERIMENTAL				// Required for using std::hash functions for the GLM types (since gtx folder contains experimental extensions)
#include <glm/glm.hpp>
//#include <glm/gtc/matrix_transform.hpp>	// Generate transformations matrices with glm::rotate (model), glm::lookAt (view), glm::perspective (projection).
//#include <glm/gtx/hash.hpp>

#include "environment.hpp"


// Prototypes ----------

struct Sizes;

struct LightSet;
struct LightPosDir;
struct LightProps;
enum lightProps;

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

struct Sizes
{
	size_t UniformAlignment = 16;	// Alignment required for each uniform in the UBO (usually, 16 bytes).
	size_t vec2 = sizeof(glm::vec2);
	size_t vec3 = sizeof(glm::vec3);
	size_t vec4 = sizeof(glm::vec4);
	size_t ivec4 = sizeof(glm::ivec4);
	size_t mat4 = sizeof(glm::mat4);
	//size_t materialSize = sizeof(Material);
	//size_t lightSize;
};

extern Sizes size;

struct Light
{
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

enum lightProps { pos = 0, dir = 1, lightType = 0, ambient, diffuse, specular, degree, cutOff };


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


struct UBOinfo
{
	UBOinfo(size_t maxNumDescriptors, size_t numActiveDescriptors, size_t minDescriptorSize)
		: maxNumDescriptors(maxNumDescriptors), numActiveDescriptors(numActiveDescriptors), minDescriptorSize(minDescriptorSize) { }
	UBOinfo() 
		: maxNumDescriptors(0), numActiveDescriptors(0), minDescriptorSize(0) { }

	size_t maxNumDescriptors;
	size_t numActiveDescriptors;
	size_t minDescriptorSize;
};


/**
*	@struct UBO
*	@brief Structure used for storing a set of UBOs in the same structure (many UBOs can be used for rendering the same model many times).
*	
*	Usual attributes of a single UBO: Model, View, Projection, ModelForNormals, Lights
*	We may create a set of dynamic UBOs (dynBlocksCount), each one containing a number of different attributes (5 max), each one containing 0 or more attributes of their type (numEachAttrib).
*	If count == 0, the buffer created will have size == range (instead of totalBytes, which is == 0). If range == 0, no buffer is created.
*	Alignments: minUBOffsetAlignment (For each dynamic UBO. Affects range), UniformAlignment (For each uniform. Affects 
*	Model matrix for Normals: Normals are passed to fragment shader in world coordinates, so they have to be multiplied by the model matrix (MM) first (this MM should not include the translation part, so we just take the upper-left 3x3 part). However, non-uniform scaling can distort normals, so we have to create a specific MM especially tailored for normal vectors: mat3(transpose(inverse(model))) * aNormal.
*	Terms: UBO (set of dynUBOs), dynUBO (set of uniforms), uniform/attribute (variables stored in a dynUBO).
*/
struct UBO
{
private:
	VulkanEnvironment* e;

public:
	UBO(VulkanEnvironment* e, UBOinfo uboInfo);			//!< Constructor. Parameters: maxUBOcount (max. number of UBOs), uboType (defines what a single UBO contains), minUBOffsetAlignment (alignment for each UBO required by the GPU).
	UBO();
	~UBO() = default;

	const size_t				maxNumDescriptors;		//!< Max. possible number of descriptors. This has to be fixed because it's fixed in the shader.
	size_t						numActiveDescriptors;	//!< Number of descriptors used (must be <= maxDescriptors). 
	VkDeviceSize				descriptorSize;			//!< Size (bytes) of each aligned descriptor (example: 4) (at least, minUBOffsetAlignment)
	size_t						totalBytes;				//!< Size (bytes) of the set of UBOs (example: 12)

	std::vector<uint8_t>		ubo;					//!< Stores the UBO that will be passed to vertex shader (MVP, M for normals, light...). Its attributes are aligned to 16-byte boundary.
	std::vector<VkBuffer>		uboBuffers;				//!< Opaque handle to a buffer object (here, uniform buffer). One for each swap chain image.
	std::vector<VkDeviceMemory>	uboMemories;			//!< Opaque handle to a device memory object (here, memory for the uniform buffer). One for each swap chain image.

	bool setNumActiveDescriptors(size_t count);			//!< Set the value of activeUBOs. Returns false if > maxUBOcount;
	uint8_t* getDescriptorPtr(size_t descriptorIndex);
	void createUBObuffers();							//!< Create uniform buffers (type of descriptors that can be bound) (VkBuffer & VkDeviceMemory), one for each swap chain image. At least one is created (if count == 0, a buffer of size "range" is created).
	void destroyUBOs();									//!< Destroy the uniform buffers (VkBuffer) and their memories (VkDeviceMemory).
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