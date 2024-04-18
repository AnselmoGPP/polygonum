#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <list>
//#include <unordered_map>			// For storing unique vertices from the model

#include <glm/glm.hpp>

//#define TINYOBJLOADER_IMPLEMENTATION	// Import OBJ models
//#include "tiny_obj_loader.h"

#include "assimp/Importer.hpp"			// Import models (vertices)
#include "assimp/scene.h"
#include "assimp/postprocess.h"

//#define STB_IMAGE_IMPLEMENTATION		// Import textures
//#include "stb_image.h"

#include "shaderc/shaderc.hpp"			// Compile shaders (GLSL code to SPIR-V)

#include "environment.hpp"
#include "vertex.hpp"


/*
	A VerticesLoader, ShaderLoader and TextureLoader objects are passed to our ModelData.
	ModelData uses it to create a ResourceLoader member.
	Later (in ModelData::fullConstruction()), ResourceLoader::loadResources() is used for loading resources.

	VerticesLoader has a VLModule* member. A children of VLModule is built, depending upon the VerticesLoader constructor used.
	ResourceLoader::loadResources()
		VerticesLoader::loadVertices()
			VLModule::loadVertices()
				getRawData() (polymorphic)
				createBuffers()
		ShaderLoader::loadShader()
		TextureLoader::loadTexture()
	
	ModelData
*/


// Definitions ----------

struct VertexData;
class VerticesLoader;
class VLModule;
	class VLM_fromFile;
	class VLM_fromBuffer;

class Shader;
class ShaderLoader;
class SLModule;
	class SLM_fromFile;
	class SLM_fromBuffer;
enum shaderModifier;
class ShaderIncluder;

class Texture;
class TextureLoader;
class TLModule;
	class TLM_fromFile;
	class TLM_fromBuffer;

struct ResourcesLoader;

class OpticalDepthTable;
class DensityVector;


// Objects ----------

extern const VertexType vt_3;					//!< (Vert)
extern const VertexType vt_32;					//!< (Vert, UV)
extern const VertexType vt_33;					//!< (Vert, Color)
extern const VertexType vt_332;					//!< (Vert, Normal, UV)
extern const VertexType vt_333;					//!< (Vert, Normal, vertexFixes)
extern const VertexType vt_3332;				//!< (Vert, Normal, Tangent, UV)

typedef std::list<Shader >::iterator shaderIter;
typedef std::list<Texture>::iterator texIter;

extern std::vector<TextureLoader> noTextures;		//!< Vector with 0 TextureLoader objects
extern std::vector<uint16_t   > noIndices;			//!< Vector with 0 indices


// Definitions ----------

// VERTICES --------------------------------------------------------

/// Vulkan Vertex data (position, color, texture coordinates...) and Indices
struct VertexData
{
	// Vertices
	uint32_t					 vertexCount;
	VkBuffer					 vertexBuffer;			//!< Opaque handle to a buffer object (here, vertex buffer).
	VkDeviceMemory				 vertexBufferMemory;	//!< Opaque handle to a device memory object (here, memory for the vertex buffer).

	// Indices
	uint32_t					 indexCount;			// <<< BUG WITH POINTS (= 7340144)
	VkBuffer					 indexBuffer;			//!< Opaque handle to a buffer object (here, index buffer).
	VkDeviceMemory				 indexBufferMemory;		//!< Opaque handle to a device memory object (here, memory for the index buffer).
};

/// (ADT) Vertices Loader Module (VLM) used in VerticesLoader for loading vertices from any source.
class VLModule
{
protected:
	const size_t vertexSize;	//!< Size (bytes) of a vertex object

	virtual void getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources) = 0;					//!< Get raw vertex data (vertices & indices)
	void createBuffers(VertexData& result, const VertexSet& rawVertices, const std::vector<uint16_t>& rawIndices, VulkanEnvironment* e);	//!< Upload raw vertex data to Vulkan (i.e., create Vulkan buffers)

	void createVertexBuffer(const VertexSet& rawVertices, VertexData& result, VulkanEnvironment* e);									//!< Vertex buffer creation.
	void createIndexBuffer(const std::vector<uint16_t>& rawIndices, VertexData& result, VulkanEnvironment* e);							//!< Index buffer creation
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VulkanEnvironment* e);

	glm::vec3 getVertexTangent(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec2 uv1, const glm::vec2 uv2, const glm::vec2 uv3);

public:
	VLModule(size_t vertexSize);
	virtual ~VLModule() { };
	virtual VLModule* clone() = 0;		//!< Create a new object of children type and return its pointer.

	void loadVertices(VertexData& result, ResourcesLoader* resources, VulkanEnvironment* e);
};

class VLM_fromBuffer : public VLModule
{
	VertexSet rawVertices;
	std::vector<uint16_t> rawIndices;

	void getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources) override;

public:
	VLM_fromBuffer(const void* verticesData, size_t vertexSize, size_t vertexCount, const std::vector<uint16_t>& indices);
	VLModule* clone() override;
};

/// Process a graphics file (obj, ...) and get the meshes. <<< Problem: This takes all the meshes in each node and stores them together. However, meshes from different nodes have their own indices, all of them in the range [0, number of vertices in the mesh). Since each mesh is an independent object, they cannot be put together without messing up with the indices (they should be stored as different models). 
class VLM_fromFile : public VLModule
{
	std::string path;

	VertexSet* vertices;
	std::vector<uint16_t>* indices;
	ResourcesLoader* resources;

	void processNode(const aiScene* scene, aiNode* node);					//!< Recursive function. It goes through each node getting all the meshes in each one.
	void processMeshes(const aiScene* scene, std::vector<aiMesh*>& meshes);	//!< Get Vertex data, Indices, and Resources (textures).

	void getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources) override;

public:
	VLM_fromFile(std::string& filePath);	//!< vertexSize == (3+3+2) * sizeof(float)

	VLModule* clone() override;
};

/// Wrapper around VLModule for loading vertices from any source. It creates a VLModule children, depending upon the constructor called, which will be used for loading vertices later (loadVertices). 
class VerticesLoader
{
	VLModule* loader;

public:
	VerticesLoader();
	VerticesLoader(std::string& filePath);	//!< From file (vertexSize == (3+3+2) * sizeof(float))
	VerticesLoader(size_t vertexSize, const void* verticesData, size_t vertexCount, std::vector<uint16_t>& indices);	//!< From buffers
	VerticesLoader(const VerticesLoader& obj);	//!< Copy constructor (necessary because loader can be freed in destructor)
	~VerticesLoader();

	void loadVertices(VertexData& result, ResourcesLoader* resources, VulkanEnvironment* e);
};


// SHADER --------------------------------------------------------

class Shader
{
public:
	Shader(VulkanEnvironment& e, const std::string id, VkShaderModule shaderModule);
	~Shader();

	VulkanEnvironment& e;						//!< Used in destructor.
	const std::string id;						//!< Used for checking whether a shader to load is already loaded.
	unsigned counter;							//!< Number of ModelData objects using this shader.
	const VkShaderModule shaderModule;
};

/// Defines some changes that can be done to the shader before compilation.
enum shaderModifier { 
	sm_albedo, 
	sm_normal, 
	sm_specular, 
	sm_roughness, 
	sm_backfaceNormals,
	sm_sunfaceNormals,
	sm_discardAlpha, 
	sm_verticalNormals,
	sm_waving_weak,
	sm_waving_strong,
	sm_displace, 
	sm_reduceNightLight,
	sm_distDithering_near,
	sm_distDithering_far,
	sm_earlyDepthTest,
	sm_dryColor
};

class SLModule		/// Shader Loader Module
{
	std::vector<shaderModifier> mods;				//!< Modifications to the shader.
	void applyModifications(std::string& shader);	//!< Applies modifications defined by "mods".

	bool findStrAndErase(std::string& text, const std::string& str);									//!< Find string and erase it.
	bool findStrAndReplace(std::string& text, const std::string& str, const std::string& replacement);	//!< Find string and replace it with another.
	bool findTwoAndReplaceBetween(std::string& text, const std::string& str1, const std::string& str2, const std::string& replacement);	//!< Find two sub-strings and replace what is in between the beginning of each sub-string.

protected:
	std::string id;

	virtual void getRawData(std::string& glslData) = 0;

public:
	SLModule(const std::string& id, std::vector<shaderModifier>& modifications);
	virtual ~SLModule() { };

	std::list<Shader>::iterator loadShader(std::list<Shader>& loadedShaders, VulkanEnvironment* e);	//!< Get an iterator to the shader in loadedShaders. If it's not in that list, it loads it, saves it in the list, and gets the iterator. 
	virtual SLModule* clone() = 0;
};

class SLM_fromBuffer : public SLModule
{
	std::string data;

	void getRawData(std::string& glslData) override;

public:
	SLM_fromBuffer(const std::string& id, const std::string& glslText, std::vector<shaderModifier>& modifications);
	SLModule* clone() override;
};

class SLM_fromFile : public SLModule
{
	std::string filePath;

	void getRawData(std::string& glslData) override;

public:
	SLM_fromFile(const std::string& filePath, std::vector<shaderModifier>& modifications);
	SLModule* clone() override;
};

/// Wrapper around SLModule for loading shaders from any source.
class ShaderLoader
{
	SLModule* loader;

public:
	ShaderLoader(const std::string& filePath, std::vector<shaderModifier>& modifications = std::vector<shaderModifier>());						//!< From file
	ShaderLoader(const std::string& id, const std::string& text, std::vector<shaderModifier>& modifications = std::vector<shaderModifier>());	//!< From buffer
	ShaderLoader();													//!< Default constructor
	ShaderLoader(const ShaderLoader& obj);							//!< Copy constructor (necessary because loader can be freed in destructor)
	~ShaderLoader();

	std::list<Shader>::iterator loadShader(std::list<Shader>& loadedShaders, VulkanEnvironment& e);
};

/**
	Includer interface for being able to "#include" headers data on shaders
	Renderer::newShader():
		- readFile(shader)
		- shaderc::CompileOptions < ShaderIncluder
		- shaderc::Compiler::PreprocessGlsl()
			- Preprocessor directive exists?
				- ShaderIncluder::GetInclude()
				- ShaderIncluder::ReleaseInclude()
				- ShaderIncluder::~ShaderIncluder()
		- shaderc::Compiler::CompileGlslToSpv
*/
class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
{
public:
	~ShaderIncluder() { };

	// Handles shaderc_include_resolver_fn callbacks.
	shaderc_include_result* GetInclude(const char* sourceName, shaderc_include_type type, const char* destName, size_t includeDepth) override;

	// Handles shaderc_include_result_release_fn callbacks.
	void ReleaseInclude(shaderc_include_result* data) override;
};


// TEXTURE --------------------------------------------------------

class Texture
{
public:
	Texture(VulkanEnvironment& e, const std::string& id, VkImage textureImage, VkDeviceMemory textureImageMemory, VkImageView textureImageView, VkSampler textureSampler);
	~Texture();

	VulkanEnvironment& e;						//!< Used in destructor.
	const std::string id;						//!< Used for checking whether the texture to load is already loaded.
	unsigned counter;							//!< Number of ModelData objects using this texture.

	VkImage				textureImage;			//!< Opaque handle to an image object.
	VkDeviceMemory		textureImageMemory;		//!< Opaque handle to a device memory object.
	VkImageView			textureImageView;		//!< Image view for the texture image (images are accessed through image views rather than directly).
	VkSampler			textureSampler;			//!< Opaque handle to a sampler object (it applies filtering and transformations to a texture). It is a distinct object that provides an interface to extract colors from a texture. It can be applied to any image you want (1D, 2D or 3D).
};

class TLModule		/// Texture Loader Module
{
protected:
	std::string id;
	VkFormat imageFormat;
	VkSamplerAddressMode addressMode;

	VulkanEnvironment* e;

	virtual void getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight) = 0;	//!< Get pixels, texWidth, texHeight, 

	std::pair<VkImage, VkDeviceMemory> createTextureImage(unsigned char* pixels, int32_t texWidth, int32_t texHeight, uint32_t& mipLevels);
	VkImageView                        createTextureImageView(VkImage textureImage, uint32_t mipLevels);
	VkSampler                          createTextureSampler(uint32_t mipLevels);

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

public:
	TLModule(const std::string& id, VkFormat imageFormat, VkSamplerAddressMode addressMode);
	virtual ~TLModule() { };

	std::list<Texture>::iterator loadTexture(std::list<Texture>& loadedTextures, VulkanEnvironment* e);	//!< Get an iterator to the Texture in loadedTextures list. If it's not in that list, it loads it, saves it in the list, and gets the iterator. 
	virtual TLModule* clone() = 0;
};

class TLM_fromBuffer : public TLModule
{
	std::vector<unsigned char> data;
	int32_t texWidth, texHeight;

	void getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight) override;

public:
	TLM_fromBuffer(const std::string& id, unsigned char* pixels, int texWidth, int texHeight, VkFormat imageFormat, VkSamplerAddressMode addressMode);
	TLModule* clone() override;
};

class TLM_fromFile : public TLModule
{
	std::string filePath;

	void getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight) override;

public:
	TLM_fromFile(const std::string& filePath, VkFormat imageFormat, VkSamplerAddressMode addressMode);
	TLModule* clone() override;
};

/// Wrapper around TLModule for loading textures from any source.
class TextureLoader
{
	TLModule* loader;

public:
	TextureLoader(std::string filePath, VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
	TextureLoader(unsigned char* pixels, int texWidth, int texHeight, std::string id, VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
	TextureLoader();	//!< Default constructor
	TextureLoader(const TextureLoader& obj);							//!< Copy constructor (necessary because loader can be freed in destructor)
	~TextureLoader();

	std::list<Texture>::iterator loadTexture(std::list<Texture>& loadedTextures, VulkanEnvironment& e);
};


// RESOURCES --------------------------------------------------------

/// Encapsulates data required for loading resources (vertices, indices, shaders, textures) and loading methods.
struct ResourcesLoader
{
	ResourcesLoader(VerticesLoader& verticesLoader, std::vector<ShaderLoader>& shadersInfo, std::vector<TextureLoader>& texturesInfo, VulkanEnvironment* e);

	VulkanEnvironment* e;
	VerticesLoader vertices;
	std::vector<ShaderLoader> shaders;
	std::vector<TextureLoader> textures;

	/// Load resources (vertices, indices, shaders, textures) and upload them to Vulkan. If a shader or texture exists in Renderer, it just takes the iterator.
	void loadResources(VertexData& destVertexData, std::vector<shaderIter>& destShaders, std::list<Shader>& loadedShaders, std::vector<texIter>& destTextures, std::list<Texture>& loadedTextures, std::mutex& mutResources);
};


// OTHERS --------------------------------------------------------

/// Precompute all optical depth values through the atmosphere. Useful for creating a lookup table for atmosphere rendering.
class OpticalDepthTable
{
	glm::vec3 planetCenter{ 0.f, 0.f, 0.f };
	unsigned planetRadius;
	unsigned atmosphereRadius;
	unsigned numOptDepthPoints;
	float heightStep;
	float angleStep;
	float densityFallOff;

	float opticalDepth(glm::vec3 rayOrigin, glm::vec3 rayDir, float rayLength) const;
	float densityAtPoint(glm::vec3 point) const;
	glm::vec2 raySphere(glm::vec3 rayOrigin, glm::vec3 rayDir) const;

public:
	OpticalDepthTable(unsigned numOptDepthPoints, unsigned planetRadius, unsigned atmosphereRadius, float heightStep, float angleStep, float densityFallOff);

	std::vector<unsigned char> table;
	size_t heightSteps;
	size_t angleSteps;
	size_t bytes;
};

/// Precompute all density values through the atmosphere. Useful for creating a lookup table for atmosphere rendering.
class DensityVector
{
public:
	DensityVector(float planetRadius, float atmosphereRadius, float stepSize, float densityFallOff);

	std::vector<unsigned char> table;
	size_t heightSteps;
	size_t bytes;
};


#endif