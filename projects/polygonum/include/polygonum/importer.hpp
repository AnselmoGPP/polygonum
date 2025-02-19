#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include "polygonum/environment.hpp"

/*
	1. A VertexesLoader, ShaderLoader and TextureLoader objects are passed to our ModelData.
	2. ModelData uses it to create a ResourceLoader member.
	3. Later, in ModelData::fullConstruction(), ResourceLoader::loadResources() is used for loading resources.

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

// Forward declarations ----------

class Renderer;
class ModelData;

// Declarations ----------

class VertexType;
class VertexSet;
struct VertexData;
class VertexesLoader;
	class VL_fromFile;
	class VL_fromBuffer;

class Shader;
class SMod;
class ShaderLoader;
class SLModule;
	class SLM_fromFile;
	class SLM_fromBuffer;
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

/// VertexType defines the characteristics of a vertex. Defines the size and type of attributes the vertex is made of (Position, Color, Texture coordinates, Normals...).
class VertexType
{
public:
	VertexType(std::initializer_list<uint32_t> attribsSizes, std::initializer_list<VkFormat> attribsFormats);	//!< Constructor. Set the size (bytes) and type of each vertex attribute (Position, Color, Texture coords, Normal, other...).
	VertexType();
	~VertexType();
	VertexType& operator=(const VertexType& obj);				//!< Copy assignment operator overloading. Required for copying a VertexSet object.

	VkVertexInputBindingDescription getBindingDescription() const;						//!< Used for passing the binding number and the vertex stride (usually, vertexSize) to the graphics pipeline.
	std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() const;	//!< Used for passing the format, location and offset of each vertex attribute to the graphics pipeline.

	std::vector<VkFormat> attribsFormats;			//!< Format (VkFormat) of each vertex attribute. E.g.: VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT...
	std::vector<uint32_t> attribsSizes;				//!< Size of each attribute type. E.g.: 3 * sizeof(float)...
	uint32_t vertexSize;								//!< Size (bytes) of a vertex object
};

/// VertexSet serves as a container for any object type, similarly to a std::vector, but storing such objects directly in bytes (char array). This allows ModelData objects store different Vertex types in a clean way (otherwise, templates and inheritance would be required, but code would be less clean).
class VertexSet
{
public:
	VertexSet();
	VertexSet(size_t vertexSize);
	~VertexSet();
	VertexSet& operator=(const VertexSet& obj);	// Not used
	VertexSet(const VertexSet& obj);			//!< Copy constructor

	size_t vertexSize;

	size_t totalBytes() const;
	size_t size() const;
	char* data() const;
	void push_back(const void* element);
	void reserve(unsigned size);
	void reset(uint32_t vertexSize, uint32_t numOfVertex, const void* buffer);	//!< Similar to a copy constructor, but just using its parameters instead of an already existing object.
	void reset(uint32_t vertexSize);

	// Debugging purposes
	void* getElement(size_t i) const;		//!< Get pointer to element
	void printElement(size_t i) const;		//!< Print vertex floats
	void printAllElements() const;	//!< Print vertex floats of all elements
	uint32_t getNumVertex() const;

private:
	char* buffer;			// Set of vertex objects stored directly in bytes
	unsigned int capacity;	// (resizable) Maximum number of vertex objects that fit in buffer
	uint32_t numVertex;		// Number of vertex objects stored in buffer
};

/// Container for Vertexes (position, color, texture coordinates...) and Indices.
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

/// Apply modifications to vertices right after loading them. Assumes vertexes start with position and then normals.
class VerticesModifier
{
protected:
	glm::vec4 params;   // Used for scaling, rotating, or translating

public:
	VerticesModifier(glm::vec4 params);
	virtual ~VerticesModifier();

	virtual void modify(VertexSet& rawVertices) = 0;
};

class VerticesModifier_Scale : public VerticesModifier
{
public:
	VerticesModifier_Scale(glm::vec3 scale);

	void modify(VertexSet& rawVertices) override;
	static VerticesModifier_Scale* factory(glm::vec3 scale);
};

class VerticesModifier_Rotation : public VerticesModifier
{
public:
	VerticesModifier_Rotation(glm::vec4 rotationQuaternion);

	void modify(VertexSet& rawVertices) override;
	static VerticesModifier_Rotation* factory(glm::vec4 rotation);
};

class VerticesModifier_Translation : public VerticesModifier
{
public:
	VerticesModifier_Translation(glm::vec3 position);

	void modify(VertexSet& rawVertices) override;
	static VerticesModifier_Translation* factory(glm::vec3 position);
};

/// ADT for loading vertices from any source. Subclasses will define how data is taken from source (getRawData): from file, from buffer, etc.
class VertexesLoader
{
protected:
	VertexesLoader(size_t vertexSize, std::initializer_list<VerticesModifier*> modifiers);

	const uint32_t vertexSize;	//!< Size (bytes) of a vertex object
	std::vector<VerticesModifier*> modifiers;
	
	virtual void getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources) = 0;   //!< Get vertexes and indices from source. Subclasses define this.
	void createBuffers(VertexData& result, const VertexSet& rawVertices, const std::vector<uint16_t>& rawIndices, Renderer& r);	//!< Upload raw vertex data to Vulkan (i.e., create Vulkan buffers)
	void applyModifiers(VertexSet& vertexes);

	void createVertexBuffer(const VertexSet& rawVertices, VertexData& result, Renderer& r);									//!< Vertex buffer creation.
	void createIndexBuffer(const std::vector<uint16_t>& rawIndices, VertexData& result, Renderer& r);							//!< Index buffer creation

	glm::vec3 getVertexTangent(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec2 uv1, const glm::vec2 uv2, const glm::vec2 uv3);

public:
	virtual ~VertexesLoader();
	virtual VertexesLoader* clone() = 0;		//!< Create a new object of children type and return its pointer.

	void loadVertexes(VertexData& result, ResourcesLoader* resources, Renderer& r);   //!< Get vertexes from source and store them in "result" ("resources" is used to store additional resources, if they exist).
};

/// Pass all the vertices at construction time. Call to getRawData will pass these vertices.
class VL_fromBuffer : public VertexesLoader
{
	VL_fromBuffer(const void* verticesData, uint32_t vertexSize, uint32_t vertexCount, const std::vector<uint16_t>& indices, std::initializer_list<VerticesModifier*> modifiers);

	VertexSet rawVertices;
	std::vector<uint16_t> rawIndices;

	void getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources) override;

public:
	static VL_fromBuffer* factory(const void* verticesData, size_t vertexSize, size_t vertexCount, const std::vector<uint16_t>& indices, std::initializer_list<VerticesModifier*> modifiers = {});
	VertexesLoader* clone() override;
};

/// Call to getRawData process a graphics file (OBJ, ...) and gets the meshes. Assumes vertexes are: position, normal, texture coordinates. <<< Problem: This takes all the meshes in each node and stores them together. However, meshes from different nodes have their own indices, all of them in the range [0, number of vertices in the mesh). Since each mesh is an independent object, they cannot be put together without messing up with the indices (they should be stored as different models). 
class VL_fromFile : public VertexesLoader
{
	VL_fromFile(std::string filePath, std::initializer_list<VerticesModifier*> modifiers);	//!< vertexSize == (3+3+2) * sizeof(float)

	std::string path;

	VertexSet* vertices;
	std::vector<uint16_t>* indices;
	ResourcesLoader* resources;   //!< Stores textures in case they're included by the file

	void processNode(const aiScene* scene, aiNode* node);					//!< Recursive function. It goes through each node getting all the meshes in each one.
	void processMeshes(const aiScene* scene, std::vector<aiMesh*>& meshes);	//!< Get Vertex data, Indices, and Resources (textures).

	void getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ResourcesLoader& destResources) override;

public:
	static VL_fromFile* factory(std::string filePath, std::initializer_list<VerticesModifier*> modifiers = {});	//!< From file (vertexSize == (3+3+2) * sizeof(float))
	VertexesLoader* clone() override;
};


// SHADER --------------------------------------------------------

// Container for a shader.
class Shader : public InterfaceForPointersManagerElements<std::string, Shader>
{
public:
	Shader(VulkanCore& c, const std::string id, VkShaderModule shaderModule);
	~Shader();

	VulkanCore& c;   //!< Used in destructor.
	const std::string id;   //!< Used for checking whether a shader to load is already loaded.
	const VkShaderModule shaderModule;
};

/// Shader modification. Change that can be applied to a shader (via applyModification()) before compilation (preprocessing operations). Constructible through a factory method. 
class SMod
{
public:
	bool applyModification(std::string& shader);
	unsigned getModType();
	std::vector<std::string> getParams();

	// Factory methods
	static SMod none();   // nothing changes
	static SMod albedo(std::string index);   // get albedo map from texture sampler
	static SMod specular(std::string index);   // get specular map from texture sampler
	static SMod roughness(std::string index);   // get roughness map from texture sampler
	static SMod normal();   // get normal map from texture sampler
	static SMod discardAlpha();   // discard fragments with less than X alpha value
	static SMod backfaceNormals();   // 
	static SMod sunfaceNormals();   // 
	static SMod verticalNormals();   // 
	static SMod wave(std::string speed, std::string amplitude, std::string minHeight);   // make mesh wave (sine wave)
	static SMod distDithering(std::string near, std::string far);   // apply dithering to distant objects
	static SMod earlyDepthTest();   // 
	static SMod dryColor(std::string color, std::string minHeight, std::string maxHeight);   // 
	static SMod changeHeader(std::string path);   // 

private:
	SMod(unsigned modificationType, std::initializer_list<std::string> params = { });

	bool findStrAndErase(std::string& text, const std::string& str);										//!< Find string and erase it.
	bool findStrAndReplace(std::string& text, const std::string& str, const std::string& replacement);		//!< Find string and replace it with another.
	bool findStrAndReplaceLine(std::string& text, const std::string& str, const std::string& replacement);	//!< Find string and replace from beginning of string to end-of-line.
	bool findTwoAndReplaceBetween(std::string& text, const std::string& str1, const std::string& str2, const std::string& replacement);	//!< Find two sub-strings and replace what is in between the beginning of string 1 and end of string 2.

	unsigned modificationType;
	std::vector<std::string> params;
};

/// ADT for loading a shader from any source. Subclasses will define how data is taken from source (getRawData): from file, from buffer, etc.
class ShaderLoader
{
	std::vector<SMod> mods;				//!< Modifications to the shader.
	void applyModifications(std::string& shader);	//!< Applies modifications defined by "mods".

protected:
	ShaderLoader(const std::string& id, const std::initializer_list<SMod>& modifications);
	virtual void getRawData(std::string& glslData) = 0;

	std::string id;

public:
	virtual ~ShaderLoader() { };

	std::shared_ptr<Shader> loadShader(PointersManager<std::string, Shader>& loadedShaders, VulkanCore& c);	//!< Get an iterator to the shader in loadedShaders. If it's not in that list, it loads it, saves it in the list, and gets the iterator. 
	virtual ShaderLoader* clone() = 0;
};

/// Pass the shader as a string at construction time. Call to getRawData will pass that string.
class SL_fromBuffer : public ShaderLoader
{
	SL_fromBuffer(const std::string& id, const std::string& glslText, const std::initializer_list<SMod>& modifications);
	void getRawData(std::string& glslData) override;

	std::string data;

public:
	static SL_fromBuffer* factory(std::string id, const std::string& glslText, std::initializer_list<SMod> modifications = {});
	ShaderLoader* clone() override;
};

/// Pass a text file path at construction time. Call to getRawData gets the shader from that file.
class SL_fromFile : public ShaderLoader
{
	SL_fromFile(const std::string& filePath, std::initializer_list<SMod>& modifications);
	void getRawData(std::string& glslData) override;

	std::string filePath;

public:
	static SL_fromFile* factory(std::string filePath, std::initializer_list<SMod> modifications = {});
	ShaderLoader* clone() override;
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

/// Container for a texture.
class Texture : public InterfaceForPointersManagerElements<std::string, Texture>
{
public:
	Texture(const std::string& id, VulkanCore& c, VkImage textureImage, VkDeviceMemory textureImageMemory, VkImageView textureImageView, VkSampler textureSampler);
	~Texture();

	//VulkanCore& c;								//!< Used in destructor.
	const std::string id;						//!< Used for checking whether the texture to load is already loaded.

	Image texture;

	//VkImage				textureImage;			//!< Opaque handle to an image object.
	//VkDeviceMemory		textureImageMemory;		//!< Opaque handle to a device memory object.
	//VkImageView			textureImageView;		//!< Image view for the texture image (images are accessed through image views rather than directly).
	//VkSampler				textureSampler;			//!< Opaque handle to a sampler object (it applies filtering and transformations to a texture). It is a distinct object that provides an interface to extract colors from a texture. It can be applied to any image you want (1D, 2D or 3D).
};

/// ADT for loading a texture from any source. Subclasses will define how data is taken from source (getRawData): from file, from buffer, etc.
class TextureLoader
{
protected:
	TextureLoader(const std::string& id, VkFormat imageFormat, VkSamplerAddressMode addressMode);

	virtual void getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight) = 0;	//!< Get pixels, texWidth, texHeight, 

	std::pair<VkImage, VkDeviceMemory> createTextureImage(unsigned char* pixels, int32_t texWidth, int32_t texHeight, uint32_t& mipLevels, Renderer& r);
	VkImageView                        createTextureImageView(VkImage textureImage, uint32_t mipLevels, VulkanCore& c);
	VkSampler                          createTextureSampler(uint32_t mipLevels, VulkanCore& c);

	std::string id;
	VkFormat imageFormat;
	VkSamplerAddressMode addressMode;

public:
	virtual ~TextureLoader() { };
	std::shared_ptr<Texture> loadTexture(PointersManager<std::string, Texture>& loadedTextures, Renderer& r);	//!< Get an iterator to the Texture in loadedTextures list. If it's not in that list, it loads it, saves it in the list, and gets the iterator. 
	virtual TextureLoader* clone() = 0;
};

/// Pass the texture as vector of bytes (unsigned char) at construction time. Call to getRawData will pass that string.
class TL_fromBuffer : public TextureLoader
{
	TL_fromBuffer(const std::string& id, unsigned char* pixels, int texWidth, int texHeight, VkFormat imageFormat, VkSamplerAddressMode addressMode);
	void getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight) override;

	std::vector<unsigned char> data;
	int32_t texWidth, texHeight;

public:
	static TL_fromBuffer* factory(const std::string id, unsigned char* pixels, int texWidth, int texHeight, VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
	TextureLoader* clone() override;
};

/// Pass a texture file path at construction time. Call to getRawData gets the texture from that file.
class TL_fromFile : public TextureLoader
{
	TL_fromFile(const std::string& filePath, VkFormat imageFormat, VkSamplerAddressMode addressMode);
	void getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight) override;

	std::string filePath;

public:
	static TL_fromFile* factory(const std::string filePath, VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
	TextureLoader* clone() override;
};


// RESOURCES --------------------------------------------------------

/// Encapsulates data required for loading resources (vertices, indices, shaders, textures) and loading methods.
struct ResourcesLoader
{
	ResourcesLoader(VertexesLoader* VertexesLoader, std::vector<ShaderLoader*>& shadersInfo, std::vector<TextureLoader*>& texturesInfo);

	std::shared_ptr<VertexesLoader> vertices;
	std::vector<ShaderLoader*> shaders;
	std::vector<TextureLoader*> textures;

	/// Get resources (vertices + indices, shaders, textures) from any source (file, buffer...) and upload them to Vulkan. If a shader or texture exists in Renderer, it just takes the iterator. As a result, `ModelData` can get the Vulkan buffers (`VertexData`, `shaderIter`s, `textureIter`s).
	void loadResources(ModelData& model, Renderer& rend);
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