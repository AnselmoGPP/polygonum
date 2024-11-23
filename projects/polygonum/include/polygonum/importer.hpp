#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include "polygonum/environment.hpp"
#include "polygonum/vertex.hpp"


/*
	A VertexesLoader, ShaderLoader and TextureLoader objects are passed to our ModelData.
	ModelData uses it to create a ResourceLoader member.
	Later (in ModelData::fullConstruction()), ResourceLoader::loadResources() is used for loading resources.

	VertexesLoader has a VLModule* member. A children of VLModule is built, depending upon the VertexesLoader constructor used.
	ResourceLoader::loadResources()
		VertexesLoader::loadVertices()
			VLModule::loadVertices()
				getRawData() (polymorphic)
				createBuffers()
		ShaderLoader::loadShader()
		TextureLoader::loadTexture()
	
	ModelData
*/


// Declarations ----------

struct VertexData;
class VertexesLoader;
	class VL_fromFile;
	class VL_fromBuffer;

class Shader;
class ShaderLoader;
class SLModule;
	class SLM_fromFile;
	class SLM_fromBuffer;
enum smFlag;
class ShaderModifier;
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

class VerticesModifier
{
protected:
	glm::vec3 params;   // Used for scaling, rotating, or translating

public:
	VerticesModifier(glm::vec3 params);
	virtual ~VerticesModifier();

	virtual void modify(VertexSet& rawVertices) = 0;
};

class VerticesModifier_Scale : public VerticesModifier
{
public:
	VerticesModifier_Scale(glm::vec3 scale);

	void modify(VertexSet& rawVertices) override;
	static VerticesModifier_Scale* factory(glm::vec3 scale);   //!< Factory function
};

class VerticesModifier_Rotation : public VerticesModifier
{
public:
	VerticesModifier_Rotation(glm::vec3 rotation);

	void modify(VertexSet& rawVertices) override;
	static VerticesModifier_Rotation* factory(glm::vec3 rotation);   //!< Factory function
};

class VerticesModifier_Translation : public VerticesModifier
{
public:
	VerticesModifier_Translation(glm::vec3 position);

	void modify(VertexSet& rawVertices) override;
	static VerticesModifier_Translation* factory(glm::vec3 position);   //!< Factory function
};

/// (ADT) Vertices Loader Module (VLM) used in VertexesLoader for loading vertices from any source.
class VertexesLoader
{
protected:
	VertexesLoader(size_t vertexSize, std::initializer_list<VerticesModifier*> modifiers);

	const size_t vertexSize;	//!< Size (bytes) of a vertex object
	std::vector<VerticesModifier*> modifiers;

	virtual void getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources) = 0;					//!< Get raw vertex data (vertices & indices)
	void createBuffers(VertexData& result, const VertexSet& rawVertices, const std::vector<uint16_t>& rawIndices, VulkanEnvironment* e);	//!< Upload raw vertex data to Vulkan (i.e., create Vulkan buffers)

	void createVertexBuffer(const VertexSet& rawVertices, VertexData& result, VulkanEnvironment* e);									//!< Vertex buffer creation.
	void createIndexBuffer(const std::vector<uint16_t>& rawIndices, VertexData& result, VulkanEnvironment* e);							//!< Index buffer creation
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VulkanEnvironment* e);

	glm::vec3 getVertexTangent(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec2 uv1, const glm::vec2 uv2, const glm::vec2 uv3);

public:
	virtual ~VertexesLoader();
	virtual VertexesLoader* clone() = 0;		//!< Create a new object of children type and return its pointer.

	void loadVertices(VertexData& result, ResourcesLoader* resources, VulkanEnvironment* e);
};

class VL_fromBuffer : public VertexesLoader
{
	VL_fromBuffer(const void* verticesData, size_t vertexSize, size_t vertexCount, const std::vector<uint16_t>& indices, std::initializer_list<VerticesModifier*> modifiers = {});

	VertexSet rawVertices;
	std::vector<uint16_t> rawIndices;

	void getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources) override;

public:
	static VL_fromBuffer* factory(const void* verticesData, size_t vertexSize, size_t vertexCount, const std::vector<uint16_t>& indices, std::initializer_list<VerticesModifier*> modifiers = {});
	VertexesLoader* clone() override;
};

/// Process a graphics file (obj, ...) and get the meshes. <<< Problem: This takes all the meshes in each node and stores them together. However, meshes from different nodes have their own indices, all of them in the range [0, number of vertices in the mesh). Since each mesh is an independent object, they cannot be put together without messing up with the indices (they should be stored as different models). 
class VL_fromFile : public VertexesLoader
{
	VL_fromFile(std::string filePath, std::initializer_list<VerticesModifier*> modifiers = {});	//!< vertexSize == (3+3+2) * sizeof(float)

	std::string path;

	VertexSet* vertices;
	std::vector<uint16_t>* indices;
	ResourcesLoader* resources;

	void processNode(const aiScene* scene, aiNode* node);					//!< Recursive function. It goes through each node getting all the meshes in each one.
	void processMeshes(const aiScene* scene, std::vector<aiMesh*>& meshes);	//!< Get Vertex data, Indices, and Resources (textures).

	void getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources) override;

public:
	static VL_fromFile* factory(std::string filePath, std::initializer_list<VerticesModifier*> modifiers = {});	//!< From file (vertexSize == (3+3+2) * sizeof(float))
	VertexesLoader* clone() override;
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

/// Shader modifier flags. It defines some changes that can be done to the shader before compilation (preprocessing operations).
enum smFlag { 
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
	sm_distDithering_near,   // apply dithering to distant objects
	sm_distDithering_far,   // apply dithering to distant objects
	sm_earlyDepthTest,
	sm_dryColor,
	sm_changeHeader,
	sm_none
};

struct ShaderModifier
{
	ShaderModifier() : flag(sm_none) { }
	ShaderModifier(smFlag flag) : flag(flag) { }
	ShaderModifier(smFlag flag, std::initializer_list<std::string> parameters) : flag(flag), params(parameters) { }

	smFlag flag;
	std::vector<std::string> params;
};

class SLModule		/// Shader Loader Module
{
	std::vector<ShaderModifier> mods;				//!< Modifications to the shader.
	void applyModifications(std::string& shader);	//!< Applies modifications defined by "mods".

	bool findStrAndErase(std::string& text, const std::string& str);										//!< Find string and erase it.
	bool findStrAndReplace(std::string& text, const std::string& str, const std::string& replacement);		//!< Find string and replace it with another.
	bool findStrAndReplaceLine(std::string& text, const std::string& str, const std::string& replacement);	//!< Find string and replace from beginning of string to end-of-line.
	bool findTwoAndReplaceBetween(std::string& text, const std::string& str1, const std::string& str2, const std::string& replacement);	//!< Find two sub-strings and replace what is in between the beginning of string 1 and end of string 2.

protected:
	std::string id;

	virtual void getRawData(std::string& glslData) = 0;

public:
	SLModule(const std::string& id, std::vector<ShaderModifier>& modifications);
	virtual ~SLModule() { };

	std::list<Shader>::iterator loadShader(std::list<Shader>& loadedShaders, VulkanEnvironment* e);	//!< Get an iterator to the shader in loadedShaders. If it's not in that list, it loads it, saves it in the list, and gets the iterator. 
	virtual SLModule* clone() = 0;
};

class SLM_fromBuffer : public SLModule
{
	std::string data;

	void getRawData(std::string& glslData) override;

public:
	SLM_fromBuffer(const std::string& id, const std::string& glslText, std::vector<ShaderModifier>& modifications);
	SLModule* clone() override;
};

class SLM_fromFile : public SLModule
{
	std::string filePath;

	void getRawData(std::string& glslData) override;

public:
	SLM_fromFile(const std::string& filePath, std::vector<ShaderModifier>& modifications);
	SLModule* clone() override;
};

/// Wrapper around SLModule for loading shaders from any source.
class ShaderLoader
{
	SLModule* loader;

public:
	ShaderLoader(const std::string& filePath, std::vector<ShaderModifier>& modifications = std::vector<ShaderModifier>());						//!< From file
	ShaderLoader(const std::string& id, const std::string& text, std::vector<ShaderModifier>& modifications = std::vector<ShaderModifier>());	//!< From buffer
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
	ResourcesLoader(VertexesLoader* VertexesLoader, std::vector<ShaderLoader>& shadersInfo, std::vector<TextureLoader>& texturesInfo, VulkanEnvironment* e);

	VulkanEnvironment* e;
	std::shared_ptr<VertexesLoader> vertices;
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