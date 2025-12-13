#ifndef VERTEX_HPP
#define VERTEX_HPP

#include "polygonum/commons.hpp"

class VertexType;
class VertexSet;
struct VertexData;
class VerticesModifier;
   class VerticesModifier_Scale;
   class VerticesModifier_Rotation;
   class VerticesModifier_Translation;
class VertexesLoader;
   class VL_fromBuffer;
   class VL_fromFile;
struct VertexPCT;

class BindingSet;
class Renderer;
class ModelData;

enum VertAttrib { vaPos, vaNorm, vaTan, vaCol, vaCol4, vaUv, vaFixes, vaBoneWeights, vaBoneIndices, vaInstanceTransform, vaMax };

/// VertexType defines the characteristics of a vertex: size and type of attributes the vertex is made of (Position, Color, Texture coordinates, Normals...).
class VertexType
{
	VertexType(std::initializer_list<uint32_t> attribsSizes, std::initializer_list<VkFormat> attribsFormats);	//!< Not used. Set the size (bytes) and type of each vertex attribute (Position, Color, Texture coords, Normal, other...).

	VkFormat getFormat(VertAttrib attribute);   //!< Maps VertAttrib to VkFormat.
	unsigned getSize(VkFormat format);   //!< Maps VkFormat to size (bytes).

public:
	VertexType(std::initializer_list<VertAttrib> vertexAttributes);
	VertexType();
	~VertexType();
	VertexType& operator=(const VertexType& obj);				//!< Copy assignment operator overloading. Required for copying a VertexSet object.

	VkVertexInputBindingDescription getBindingDescription() const;						//!< Used for passing the binding number and the vertex stride (usually, vertexSize) to the graphics pipeline.
	std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() const;	//!< Used for passing the format, location and offset of each vertex attribute to the graphics pipeline.

	std::vector<VkFormat> attribsFormats;			//!< Format (VkFormat) of each vertex attribute. E.g.: VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT...
	std::vector<uint32_t> attribsSizes;				//!< Size of each attribute type. E.g.: 3 * sizeof(float)...
	uint32_t vertexSize;							//!< Size (bytes) of a vertex object
	std::vector<VertAttrib> attribsTypes; // <<< set to map?
};

/// Container for any object type, similarly to a std::vector, but storing such objects directly in bytes (char array). This allows ModelData objects store different Vertex types in a clean way (otherwise, templates and inheritance would be required, but code would be less clean).
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

/// Container for buffers for Vertexes (position, color, texture coordinates...) and Indices.
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

	virtual void getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ModelData& model) = 0;   //!< Get vertexes and indices from source. Subclasses define this.
	void createBuffers(VertexData& result, const VertexSet& rawVertices, const std::vector<uint16_t>& rawIndices, Renderer& r);	//!< Upload raw vertex data to Vulkan (i.e., create Vulkan buffers)
	void applyModifiers(VertexSet& vertexes);

	void createVertexBuffer(const VertexSet& rawVertices, VertexData& result, Renderer& r);									//!< Vertex buffer creation.
	void createIndexBuffer(const std::vector<uint16_t>& rawIndices, VertexData& result, Renderer& r);							//!< Index buffer creation

	glm::vec3 getVertexTangent(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec2 uv1, const glm::vec2 uv2, const glm::vec2 uv3);

public:
	virtual ~VertexesLoader();
	virtual VertexesLoader* clone() = 0;		//!< Create a new object of children type and return its pointer.

	void loadVertexes(Renderer& r, ModelData& model);   //!< Get vertexes from source and store them in "result" ("resources" is used to store additional resources, if they exist).
};

/// Pass all the vertices at construction time. Call to getRawData will pass these vertices.
class VL_fromBuffer : public VertexesLoader
{
	VL_fromBuffer(const void* verticesData, uint32_t vertexSize, uint32_t vertexCount, const std::vector<uint16_t>& indices = { }, std::initializer_list<VerticesModifier*> modifiers = { });

	VertexSet rawVertices;
	std::vector<uint16_t> rawIndices;

	void getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ModelData& model) override;

public:
	static VL_fromBuffer* factory(const void* verticesData, size_t vertexSize, size_t vertexCount, const std::vector<uint16_t>& indices = { }, std::initializer_list<VerticesModifier*> modifiers = {});
	VertexesLoader* clone() override;
};

/// Call to getRawData process a graphics file (OBJ, ...) and gets the meshes. Assumes vertexes are: position, normal, texture coordinates. <<< Problem: This takes all the meshes in each node and stores them together. However, meshes from different nodes have their own indices, all of them in the range [0, number of vertices in the mesh). Since each mesh is an independent object, they cannot be put together without messing up with the indices (they should be stored as different models). 
class VL_fromFile : public VertexesLoader
{
	VL_fromFile(std::string filePath, std::initializer_list<VerticesModifier*> modifiers);	//!< vertexSize == (3+3+2) * sizeof(float)

	std::string path;

	VertexSet* vertices;
	std::vector<uint16_t>* indices;
	ModelData* model;   //!< Used to store textures if the file includes them.

	void processNode(const aiScene* scene, aiNode* node);					//!< Recursive function. It goes through each node getting all the meshes in each one.
	void processMeshes(const aiScene* scene, std::vector<aiMesh*>& meshes);	//!< Get Vertex data, Indices, and Resources (textures).
	void allocateMemForTextures();   //!< Add set and binding in bindSets if they doesn't exist.

	void getRawData(VertexSet& destVertices, std::vector<uint16_t>& destIndices, ModelData& model) override;

public:
	static VL_fromFile* factory(std::string filePath, std::initializer_list<VerticesModifier*> modifiers = {});	//!< From file (vertexSize == (3+3+2) * sizeof(float))
	VertexesLoader* clone() override;
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